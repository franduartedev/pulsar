#pragma once

#include <pulsar/application/waveform/waveform_renderer.hpp>

#include <QColor>
#include <QQuickItem>
#include <QString>

#include <memory>

class WaveformQuickItem final : public QQuickItem {
  Q_OBJECT
  Q_PROPERTY(QString clipKey READ clipKey WRITE setClipKey NOTIFY clipKeyChanged)
  Q_PROPERTY(bool selected READ selected WRITE setSelected NOTIFY styleChanged)
  Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY styleChanged)
  Q_PROPERTY(double density READ density WRITE setDensity NOTIFY inputChanged)
  Q_PROPERTY(int waveformState READ waveformState NOTIFY waveformStateChanged)
  Q_PROPERTY(bool hasUnavailableRange READ hasUnavailableRange NOTIFY waveformStateChanged)
  Q_PROPERTY(QColor normalColor READ normalColor WRITE setNormalColor NOTIFY styleChanged)
  Q_PROPERTY(QColor selectedColor READ selectedColor WRITE setSelectedColor NOTIFY styleChanged)
  Q_PROPERTY(QColor pendingColor READ pendingColor WRITE setPendingColor NOTIFY styleChanged)
  Q_PROPERTY(QColor unavailableColor READ unavailableColor WRITE setUnavailableColor NOTIFY styleChanged)
  Q_PROPERTY(QColor baselineColor READ baselineColor WRITE setBaselineColor NOTIFY styleChanged)
public:
  enum State { Missing, Generating, Partial, Ready, Failed };
  Q_ENUM(State)

  explicit WaveformQuickItem(QQuickItem* parent = nullptr);
  [[nodiscard]] QString clipKey() const { return clip_key_; }
  void setClipKey(QString value);
  [[nodiscard]] bool selected() const noexcept { return selected_; }
  void setSelected(bool value);
  [[nodiscard]] bool muted() const noexcept { return muted_; }
  void setMuted(bool value);
  [[nodiscard]] double density() const noexcept { return density_; }
  void setDensity(double value);
  [[nodiscard]] int waveformState() const noexcept { return state_; }
  [[nodiscard]] bool hasUnavailableRange() const noexcept { return has_unavailable_range_; }
  [[nodiscard]] QColor normalColor() const { return normal_color_; }
  void setNormalColor(QColor value);
  [[nodiscard]] QColor selectedColor() const { return selected_color_; }
  void setSelectedColor(QColor value);
  [[nodiscard]] QColor pendingColor() const { return pending_color_; }
  void setPendingColor(QColor value);
  [[nodiscard]] QColor unavailableColor() const { return unavailable_color_; }
  void setUnavailableColor(QColor value);
  [[nodiscard]] QColor baselineColor() const { return baseline_color_; }
  void setBaselineColor(QColor value);

  void setPreparedGeometry(std::shared_ptr<const pulsar::application::WaveformGeometry> geometry,
                           State state, bool has_available_waveform);

signals:
  void clipKeyChanged();
  void styleChanged();
  void inputChanged();
  void waveformStateChanged();

protected:
  [[nodiscard]] QSGNode* updatePaintNode(QSGNode* old_node,
                                         UpdatePaintNodeData*) override;

private:
  QString clip_key_;
  bool selected_{false};
  bool muted_{false};
  double density_{1.0};
  int state_{Missing};
  QColor normal_color_{QStringLiteral("#71a4ff")};
  QColor selected_color_{QStringLiteral("#c7dcff")};
  QColor pending_color_{QStringLiteral("#d3a85d")};
  QColor unavailable_color_{QStringLiteral("#69717c")};
  QColor baseline_color_{QStringLiteral("#56667b")};
  bool has_available_waveform_{false};
  bool has_unavailable_range_{false};
  std::shared_ptr<const pulsar::application::WaveformGeometry> geometry_;
};
