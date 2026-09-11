#pragma once

#include <QObject>
#include <QString>

namespace yaap {

class ModApiInfo final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    explicit ModApiInfo(QObject* parent = nullptr);
    [[nodiscard]] QString version() const;
};

} // namespace yaap
