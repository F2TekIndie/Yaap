#pragma once

#include <QObject>
#include <QString>

namespace yaap {

class ModApiInfo final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString trustWarning READ trustWarning CONSTANT)

public:
    explicit ModApiInfo(QObject* parent = nullptr);
    [[nodiscard]] QString version() const;
    [[nodiscard]] QString trustWarning() const;
};

} // namespace yaap
