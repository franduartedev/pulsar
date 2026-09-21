#include "waveform_quick_item.hpp"

#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGNode>
#include <QThread>

#include <algorithm>
#include <cmath>

namespace {

QSGGeometryNode* child_node(QSGNode* root, int child_index) {
  while (root->childCount() <= child_index) {
    auto* node = new QSGGeometryNode;
    auto* geometry = new QSGGeometry{QSGGeometry::defaultAttributes_Point2D(), 0};
    geometry->setDrawingMode(QSGGeometry::DrawLines);
    geometry->setLineWidth(1.0f);
    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    auto* material = new QSGFlatColorMaterial;
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial);
    root->appendChildNode(node);
  }
  return static_cast<QSGGeometryNode*>(root->childAtIndex(child_index));
}

void set_lines(QSGGeometryNode* node, const std::vector<QPointF>& points, QColor color) {
  auto* geometry = node->geometry();
  const int vertex_count = static_cast<int>(points.size());
  if (geometry->vertexCount() != vertex_count) geometry->allocate(vertex_count);
  auto* vertices = geometry->vertexDataAsPoint2D();
  for (int index = 0; index < vertex_count; ++index) {
    vertices[index].set(static_cast<float>(points[static_cast<std::size_t>(index)].x()),
                        static_cast<float>(points[static_cast<std::size_t>(index)].y()));
  }
  auto* material = static_cast<QSGFlatColorMaterial*>(node->material());
  material->setColor(color);
  node->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
}

} // namespace

WaveformQuickItem::WaveformQuickItem(QQuickItem* parent) : QQuickItem{parent} {
  setFlag(ItemHasContents, true);
  connect(this, &QQuickItem::widthChanged, this, &WaveformQuickItem::inputChanged);
  connect(this, &QQuickItem::heightChanged, this, &WaveformQuickItem::inputChanged);
  connect(this, &QQuickItem::visibleChanged, this, &WaveformQuickItem::inputChanged);
}

void WaveformQuickItem::setClipKey(QString value) {
  if (clip_key_ == value) return;
  clip_key_ = std::move(value);
  emit clipKeyChanged();
  emit inputChanged();
}
void WaveformQuickItem::setSelected(bool value) {
  if (selected_ == value) return;
  selected_ = value; emit styleChanged(); update();
}
void WaveformQuickItem::setMuted(bool value) {
  if (muted_ == value) return;
  muted_ = value; emit styleChanged(); update();
}
void WaveformQuickItem::setDensity(double value) {
  if (!std::isfinite(value) || value <= 0.0 || density_ == value) return;
  density_ = value; emit inputChanged();
}
void WaveformQuickItem::setNormalColor(QColor value) {
  if (normal_color_ == value) return;
  normal_color_ = std::move(value); emit styleChanged(); update();
}
void WaveformQuickItem::setSelectedColor(QColor value) {
  if (selected_color_ == value) return;
  selected_color_ = std::move(value); emit styleChanged(); update();
}
void WaveformQuickItem::setPendingColor(QColor value) {
  if (pending_color_ == value) return;
  pending_color_ = std::move(value); emit styleChanged(); update();
}
void WaveformQuickItem::setUnavailableColor(QColor value) {
  if (unavailable_color_ == value) return;
  unavailable_color_ = std::move(value); emit styleChanged(); update();
}
void WaveformQuickItem::setBaselineColor(QColor value) {
  if (baseline_color_ == value) return;
  baseline_color_ = std::move(value); emit styleChanged(); update();
}

void WaveformQuickItem::setPreparedGeometry(
    std::shared_ptr<const pulsar::application::WaveformGeometry> geometry, State state,
    bool has_available_waveform) {
  Q_ASSERT(QThread::currentThread() == thread());
  const bool state_changed = state_ != static_cast<int>(state);
  geometry_ = std::move(geometry);
  state_ = static_cast<int>(state);
  has_available_waveform_ = has_available_waveform;
  const bool unavailable = geometry_ && !geometry_->unavailable_columns.empty();
  const bool availability_changed = has_unavailable_range_ != unavailable;
  has_unavailable_range_ = unavailable;
  if (state_changed || availability_changed) emit waveformStateChanged();
  update();
}

QSGNode* WaveformQuickItem::updatePaintNode(QSGNode* old_node, UpdatePaintNodeData*) {
  if (width() <= 0.0 || height() <= 0.0 || !geometry_) return old_node;
  auto* root = old_node == nullptr ? new QSGNode : old_node;
  std::vector<QPointF> peaks;
  peaks.reserve(geometry_->segments.size() * 2U);
  for (const auto& segment : geometry_->segments) {
    const qreal x = std::min(static_cast<qreal>(segment.column) + 0.5, width() - 0.5);
    peaks.emplace_back(x, std::clamp(static_cast<qreal>(segment.top), 0.5,
                                     std::max(0.5, height() - 0.5)));
    peaks.emplace_back(x, std::clamp(static_cast<qreal>(segment.bottom), 0.5,
                                     std::max(0.5, height() - 0.5)));
  }
  QColor waveform_color = selected_ ? selected_color_ : normal_color_;
  if (muted_) waveform_color.setAlphaF(waveform_color.alphaF() * 0.45);
  set_lines(child_node(root, 0), peaks, waveform_color);

  std::vector<QPointF> baseline;
  if (has_available_waveform_ && !geometry_->segments.empty()) {
    baseline = {QPointF{0.0, height() / 2.0}, QPointF{width(), height() / 2.0}};
  }
  QColor baseline_color = baseline_color_;
  if (muted_) baseline_color.setAlphaF(baseline_color.alphaF() * 0.45);
  set_lines(child_node(root, 1), baseline, baseline_color);

  std::vector<QPointF> pending;
  pending.reserve(geometry_->pending_columns.size() / 4U * 2U);
  const qreal left_edge = std::min<qreal>(0.5, width() / 2.0);
  const qreal right_edge = std::max(left_edge, width() - 0.5);
  const qreal state_top = std::min<qreal>(2.0, height() / 2.0);
  const qreal state_bottom = std::max(state_top, std::min<qreal>(height() - 0.5, 7.0));
  for (const auto column : geometry_->pending_columns) {
    if ((column & 3U) != 0U) continue;
    const qreal x = std::min(static_cast<qreal>(column) + 0.5, width() - 0.5);
    pending.emplace_back(std::clamp(x - 1.5, left_edge, right_edge), state_top);
    pending.emplace_back(std::clamp(x + 1.5, left_edge, right_edge), state_bottom);
  }
  set_lines(child_node(root, 2), pending, pending_color_);

  std::vector<QPointF> unavailable;
  unavailable.reserve(geometry_->unavailable_columns.size() / 4U * 2U);
  for (const auto column : geometry_->unavailable_columns) {
    if ((column & 3U) != 0U) continue;
    const qreal x = std::min(static_cast<qreal>(column) + 0.5, width() - 0.5);
    unavailable.emplace_back(std::clamp(x - 1.5, left_edge, right_edge), state_top);
    unavailable.emplace_back(std::clamp(x + 1.5, left_edge, right_edge), state_bottom);
  }
  set_lines(child_node(root, 3), unavailable, unavailable_color_);
  return root;
}
