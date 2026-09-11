#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QVariantMap>

namespace yaap {
class PlayerController;
class ApplicationSession;

class MprisRootAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit CONSTANT)
    Q_PROPERTY(bool CanRaise READ canRaise CONSTANT)
    Q_PROPERTY(bool HasTrackList READ hasTrackList CONSTANT)
    Q_PROPERTY(QString Identity READ identity CONSTANT)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry CONSTANT)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes CONSTANT)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes CONSTANT)
public:
    MprisRootAdaptor(QObject* parent, ApplicationSession& session);
    bool canQuit() const { return true; }
    bool canRaise() const { return true; }
    bool hasTrackList() const { return false; }
    QString identity() const { return "Yaap"; }
    QString desktopEntry() const { return "org.yaap.Yaap"; }
    QStringList supportedUriSchemes() const { return {"file", "http", "https"}; }
    QStringList supportedMimeTypes() const;
public slots:
    void Raise();
    void Quit();
private:
    ApplicationSession& m_session;
};

class MprisPlayerAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_CLASSINFO("D-Bus Introspection", ""
        "  <interface name=\"org.mpris.MediaPlayer2.Player\">"
        "    <method name=\"Next\"/><method name=\"Previous\"/>"
        "    <method name=\"Pause\"/><method name=\"PlayPause\"/>"
        "    <method name=\"Stop\"/><method name=\"Play\"/>"
        "    <method name=\"Seek\"><arg direction=\"in\" type=\"x\" name=\"Offset\"/></method>"
        "    <method name=\"SetPosition\"><arg direction=\"in\" type=\"o\" name=\"TrackId\"/><arg direction=\"in\" type=\"x\" name=\"Position\"/></method>"
        "    <method name=\"OpenUri\"><arg direction=\"in\" type=\"s\" name=\"Uri\"/></method>"
        "    <signal name=\"Seeked\"><arg type=\"x\" name=\"Position\"/></signal>"
        "    <property name=\"PlaybackStatus\" type=\"s\" access=\"read\"/>"
        "    <property name=\"Rate\" type=\"d\" access=\"readwrite\"/>"
        "    <property name=\"Metadata\" type=\"a{sv}\" access=\"read\"/>"
        "    <property name=\"Volume\" type=\"d\" access=\"readwrite\"/>"
        "    <property name=\"Position\" type=\"x\" access=\"read\"><annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"false\"/></property>"
        "    <property name=\"MinimumRate\" type=\"d\" access=\"read\"/>"
        "    <property name=\"MaximumRate\" type=\"d\" access=\"read\"/>"
        "    <property name=\"CanGoNext\" type=\"b\" access=\"read\"/>"
        "    <property name=\"CanGoPrevious\" type=\"b\" access=\"read\"/>"
        "    <property name=\"CanPlay\" type=\"b\" access=\"read\"/>"
        "    <property name=\"CanPause\" type=\"b\" access=\"read\"/>"
        "    <property name=\"CanSeek\" type=\"b\" access=\"read\"/>"
        "    <property name=\"CanControl\" type=\"b\" access=\"read\"/>"
        "  </interface>")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(double MinimumRate READ rate CONSTANT)
    Q_PROPERTY(double MaximumRate READ rate CONSTANT)
    Q_PROPERTY(bool CanGoNext READ canGoNext CONSTANT)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious CONSTANT)
    Q_PROPERTY(bool CanPlay READ canPlay)
    Q_PROPERTY(bool CanPause READ canPlay)
    Q_PROPERTY(bool CanSeek READ canSeek)
    Q_PROPERTY(bool CanControl READ canControl CONSTANT)
public:
    MprisPlayerAdaptor(QObject* parent, PlayerController& player, QDBusConnection connection);
    QString playbackStatus() const;
    QVariantMap metadata() const;
    double volume() const;
    void setVolume(double value);
    qlonglong position() const;
    double rate() const { return 1.0; }
    void setRate(double value);
    bool canGoNext() const { return false; }
    bool canGoPrevious() const { return false; }
    bool canPlay() const;
    bool canSeek() const;
    bool canControl() const { return true; }
    QDBusObjectPath trackId() const;
public slots:
    void Next() {}
    void Previous() {}
    void Pause();
    void PlayPause();
    void Stop();
    void Play();
    void Seek(qlonglong offset);
    void SetPosition(const QDBusObjectPath& trackId, qlonglong position);
    void OpenUri(const QString& uri);
signals:
    void Seeked(qlonglong position);
private:
    void publishChanges();
    PlayerController& m_player;
    QDBusConnection m_connection;
    QVariantMap m_previousProperties;
};

class MprisService final : public QObject {
    Q_OBJECT
public:
    static constexpr auto serviceName = "org.mpris.MediaPlayer2.Yaap";
    static constexpr auto objectPath = "/org/mpris/MediaPlayer2";
    MprisService(PlayerController& player, ApplicationSession& session,
        QDBusConnection connection = QDBusConnection::sessionBus());
    ~MprisService() override;
    bool isRegistered() const { return m_registered; }
private:
    QDBusConnection m_connection;
    bool m_registered{};
};
}
