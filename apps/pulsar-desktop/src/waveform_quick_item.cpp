#include "waveform_quick_item.hpp"

#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGNode>
#include <QThread>

#include <algorithm>
#include <cmath>

namespace {

// Child layout (draw order): 0 body fill, 1 envelope edges, 2 centre axis,
// 3 pending hatch, 4 unavailable hatch, 5 bright core (mass at the loud centre).
constexpr int kBodyNode = 0;
constexpr int kEdgeNode = 1;
constexpr int kAxisNode = 2;
constexpr int kPendingNode = 3;
constexpr int kUnavailableNode = 4;
constexpr int kCoreNode = 5;
// The core covers this fraction of the envelope's half-height around the axis.
constexpr qreal kCoreFraction = 0.55;

QSGGeometryNode* child_node(QSGNode* root, int child_index) {
  while (root->childCount() <= child_index) {
    auto* node = new QSGGeometryNode;
    auto* geometry = new QSGGeometry{QSGGeometry::defaultAttributes_Point2D(), 0};
    const int index = root->childCount();
    geometry->setDrawingMode(index == kBodyNode || index == kCoreNode ? QSGGeometry::DrawTriangles
                                                                      : QSGGeometry::DrawLines);
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

// Envelope control point in item pixels (x at pixel centre).
struct EnvelopePoint {
  qreal x;
  qreal top;
  qreal bottom;
};

// A long flat run (e.g. digital silence) keeps its plateau; shorter runs are
// the per-bucket steps that appear when zoomed in, and are collapsed to their
// centre so the envelope interpolates between buckets instead of stair-stepping.
constexpr int kMaxCollapsedPlateauColumns = 24;
// Quiet audio still reads as a line: never thinner than this.
constexpr qreal kMinBodyThickness = 1.6;

QColor blend(QColor from, const QColor& to, qreal amount) {
  from.setRgbF(from.redF() + (to.redF() - from.redF()) * amount,
               from.greenF() + (to.greenF() - from.greenF()) * amount,
               from.blueF() + (to.blueF() - from.blueF()) * amount, from.alphaF());
  return from;
}

// Turns the engine's per-column extrema (never modified) into contiguous runs
// of control points. A run breaks at missing columns and at channel changes.
void build_envelope_runs(const std::vector<pulsar::application::WaveformSegment>& segments,
                         qreal height, std::vector<EnvelopePoint>& points,
                         std::vector<std::size_t>& run_starts) {
  const auto thicken = [height](qreal top, qreal bottom) {
    if (bottom - top < kMinBodyThickness) {
      const qreal mid = (top + bottom) / 2.0;
      top = mid - kMinBodyThickness / 2.0;
      bottom = mid + kMinBodyThickness / 2.0;
      if (top < 0.0) { bottom -= top; top = 0.0; }
      if (bottom > height) { top -= bottom - height; bottom = height; }
    }
    return std::pair<qreal, qreal>{std::max(0.0, top), std::min(height, bottom)};
  };
  const auto same = [](const pulsar::application::WaveformSegment& a,
                       const pulsar::application::WaveformSegment& b) {
    return std::abs(a.top - b.top) < 0.01 && std::abs(a.bottom - b.bottom) < 0.01;
  };
  std::size_t i = 0;
  const std::size_t count = segments.size();
  while (i < count) {
    // Extent of this contiguous run of columns.
    std::size_t run_end = i + 1;
    while (run_end < count && segments[run_end].column == segments[run_end - 1].column + 1U) ++run_end;
    run_starts.push_back(points.size());
    const auto add_point = [&](qreal x, const pulsar::application::WaveformSegment& seg) {
      const auto [top, bottom] = thicken(seg.top, seg.bottom);
      points.push_back({x, top, bottom});
    };
    add_point(static_cast<qreal>(segments[i].column), segments[i]);  // left edge, flat lead-in
    std::size_t group = i;
    while (group < run_end) {
      std::size_t group_end = group + 1;
      while (group_end < run_end && same(segments[group], segments[group_end])) ++group_end;
      const auto length = static_cast<int>(group_end - group);
      const qreal first_x = static_cast<qreal>(segments[group].column) + 0.5;
      if (length <= kMaxCollapsedPlateauColumns) {
        add_point(first_x + (length - 1) / 2.0, segments[group]);
      } else {
        add_point(first_x, segments[group]);
        add_point(first_x + (length - 1), segments[group]);
      }
      group = group_end;
    }
    add_point(static_cast<qreal>(segments[run_end - 1].column) + 1.0, segments[run_end - 1]);  // right edge
    i = run_end;
  }
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
  // Envelope: filled body + crisp top/bottom edges, built from the engine's
  // per-column extrema. Muted drops saturation instead of only fading.
  QColor edge_color = selected_ ? selected_color_ : normal_color_;
  QColor fill_color = edge_color;
  fill_color.setAlphaF(selected_ ? 0.95 : 0.86);
  QColor core_color = blend(edge_color, QColor{255, 255, 255}, selected_ ? 0.45 : 0.3);
  core_color.setAlphaF(selected_ ? 0.9 : 0.75);
  QColor axis_color = baseline_color_;
  axis_color.setAlphaF(axis_color.alphaF() * 0.5);
  if (muted_) {
    edge_color = blend(edge_color, QColor{0x8a, 0x93, 0xa0}, 0.8);
    edge_color.setAlphaF(edge_color.alphaF() * 0.6);
    fill_color = blend(fill_color, QColor{0x8a, 0x93, 0xa0}, 0.8);
    fill_color.setAlphaF(fill_color.alphaF() * 0.35);
    core_color = blend(core_color, QColor{0x8a, 0x93, 0xa0}, 0.8);
    core_color.setAlphaF(0.22);
    axis_color.setAlphaF(axis_color.alphaF() * 0.6);
  }

  std::vector<EnvelopePoint> envelope;
  std::vector<std::size_t> run_starts;
  envelope.reserve(geometry_->segments.size() + 8U);
  build_envelope_runs(geometry_->segments, height(), envelope, run_starts);
  run_starts.push_back(envelope.size());
  std::vector<QPointF> body;
  std::vector<QPointF> core;
  std::vector<QPointF> edges;
  body.reserve(envelope.size() * 6U);
  edges.reserve(envelope.size() * 4U);
  core.reserve(envelope.size() * 6U);
  // Same envelope pulled toward the axis: loud passages get a bright heart, quiet ones stay a thin line.
  const qreal axis_y = height() / 2.0;
  const auto toward_axis = [axis_y](qreal y) { return axis_y + (y - axis_y) * kCoreFraction; };
  for (std::size_t run = 0; run + 1 < run_starts.size(); ++run) {
    for (std::size_t k = run_starts[run]; k + 1 < run_starts[run + 1]; ++k) {
      const auto& p = envelope[k];
      const auto& q = envelope[k + 1];
      body.emplace_back(p.x, p.top);    body.emplace_back(p.x, p.bottom);  body.emplace_back(q.x, q.top);
      body.emplace_back(q.x, q.top);    body.emplace_back(p.x, p.bottom);  body.emplace_back(q.x, q.bottom);
      core.emplace_back(p.x, toward_axis(p.top));    core.emplace_back(p.x, toward_axis(p.bottom)); core.emplace_back(q.x, toward_axis(q.top));
      core.emplace_back(q.x, toward_axis(q.top));    core.emplace_back(p.x, toward_axis(p.bottom)); core.emplace_back(q.x, toward_axis(q.bottom));
      edges.emplace_back(p.x, p.top);   edges.emplace_back(q.x, q.top);
      edges.emplace_back(p.x, p.bottom); edges.emplace_back(q.x, q.bottom);
    }
  }
  set_lines(child_node(root, kBodyNode), body, fill_color);
  set_lines(child_node(root, kEdgeNode), edges, edge_color);

  std::vector<QPointF> baseline;
  if (has_available_waveform_ && !geometry_->segments.empty()) {
    baseline = {QPointF{0.0, height() / 2.0}, QPointF{width(), height() / 2.0}};
  }
  set_lines(child_node(root, kAxisNode), baseline, axis_color);

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
  set_lines(child_node(root, kPendingNode), pending, pending_color_);

  std::vector<QPointF> unavailable;
  unavailable.reserve(geometry_->unavailable_columns.size() / 4U * 2U);
  for (const auto column : geometry_->unavailable_columns) {
    if ((column & 3U) != 0U) continue;
    const qreal x = std::min(static_cast<qreal>(column) + 0.5, width() - 0.5);
    unavailable.emplace_back(std::clamp(x - 1.5, left_edge, right_edge), state_top);
    unavailable.emplace_back(std::clamp(x + 1.5, left_edge, right_edge), state_bottom);
  }
  set_lines(child_node(root, kUnavailableNode), unavailable, unavailable_color_);
  set_lines(child_node(root, kCoreNode), core, core_color);
  return root;
}
