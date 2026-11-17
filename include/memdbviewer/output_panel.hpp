#pragma once

#include <QWidget>
#include <QString>

#include <memory>

namespace memdb::viewer {
class ViewerCore;

/// @brief Ausgabe-Panel (Ausgabe/Log): zeigt alle verifizierten Operationen des
///        ViewerCore – Erfolge in schwarz, Fehler (Exceptions aus InMemDB) in rot –
///        mit Zeitstempel. Wird z. B. als Dock unten im Hauptfenster gehostet.
///
/// Verwendung: `auto* out = new OutputPanel; out->setCore(core);`
class OutputPanel : public QWidget {
    Q_OBJECT
public:
    explicit OutputPanel(QWidget* parent = nullptr);
    ~OutputPanel() override;

    void setCore(ViewerCore* core);
    ViewerCore* core() const;

public slots:
    /// @brief Eintrag ins Log (auch manuell nutzbar); `error` = rot hervorgehoben.
    void log(const QString& message, bool error = false);
    void clear();

private:
    void updateCount();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace memdb::viewer
