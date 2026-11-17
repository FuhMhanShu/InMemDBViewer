#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <vector>

namespace memdb {
class Database;
}

namespace memdb::viewer {
class ViewerCore;
namespace detail {

/// @brief Eine Schema-Feld-Definition aus dem Design-Widget.
struct FieldDef {
    QString path;
    QString typeName;   ///< memdb::typeName(Value::Type), z. B. "string", "int", "document"
    bool nullable = true;
    QString subSchema;  ///< nur fuer Typ "document": komma-getrennte Unterfelder "pfad:typ"
    QString arrayElem;  ///< nur fuer Typ "array": Element-Typname
};

/// @brief Controller (MVC) des DatabaseDesignWidgets: alle Design-Operationen laufen
///        ueber ViewerCore (verifiziert + geloggt + dataMutated). Kennt keine Widgets.
class DesignController : public QObject {
    Q_OBJECT
public:
    explicit DesignController(ViewerCore* core, QObject* parent = nullptr);

    void setCore(ViewerCore* core);
    ViewerCore* core() const { return core_; }

    QStringList collections() const;

    /// @brief aktuelle Schema-Felder einer Collection als FieldDefs (leer bei schema-los).
    std::vector<FieldDef> currentSchema(const QString& collection) const;
    /// @brief Feldnamen-Vorschläge (Schema-Felder oder distinctValues-Sample) fuer Index-Completer.
    QStringList fieldSuggestions(const QString& collection) const;

    // Operationen (verifiziert ueber ViewerCore, Fehler landen in der Ausgabe)
    void createCollection(const QString& name);
    void dropCollection(const QString& name);
    void clearCollection(const QString& name);
    void applySchema(const QString& collection, const std::vector<FieldDef>& fields);
    void removeSchema(const QString& collection);
    void addIndex(const QString& collection, const QString& fieldPath, bool unique);
    void removeIndex(const QString& collection, const QString& fieldPath);
    void addReference(const QString& fromColl, quint64 fromId, bool hasFromId, const QString& toColl,
                      quint64 toId, bool hasToId, const QString& relation);
    void removeReference(const QString& fromColl, quint64 fromId, bool hasFromId, const QString& toColl,
                         quint64 toId, bool hasToId, const QString& relation);
    void pruneDangling();

signals:
    /// @brief nach erfolgreicher Mutation: Widget-Listen neu laden.
    void changed();

private:
    ViewerCore* core_ = nullptr;
};

} // namespace detail
} // namespace memdb::viewer
