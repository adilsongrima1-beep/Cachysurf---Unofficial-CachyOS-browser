#pragma once

#include <QHash>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include <hidapi/hidapi.h>

class QTimer;
class QWidget;
class QWebEnginePage;
class HidBridge;

class HidPageBridge final : public QObject
{
    Q_OBJECT

public:
    HidPageBridge(HidBridge *bridge, QWebEnginePage *page, QObject *parent = nullptr);

    Q_INVOKABLE QVariantMap requestDevices(const QString &optionsJson);
    Q_INVOKABLE QVariantMap getDevices();
    Q_INVOKABLE QVariantMap openDevice(const QString &deviceId);
    Q_INVOKABLE QVariantMap closeDevice(const QString &deviceId);
    Q_INVOKABLE QVariantMap sendReport(const QString &deviceId, int reportId, const QVariantList &data);
    Q_INVOKABLE QVariantMap sendFeatureReport(const QString &deviceId, int reportId, const QVariantList &data);
    Q_INVOKABLE QVariantMap receiveFeatureReport(const QString &deviceId, int reportId, int length);
    Q_INVOKABLE QVariantMap forgetDevice(const QString &deviceId);

signals:
    void inputReport(const QString &deviceId, int reportId, const QVariantList &data);
    void deviceDisconnected(const QString &deviceId);

private:
    QString currentOrigin() const;

    HidBridge *m_bridge = nullptr;
    QWebEnginePage *m_page = nullptr;
};

class HidBridge final : public QObject
{
    Q_OBJECT

public:
    explicit HidBridge(QWidget *dialogParent, QObject *parent = nullptr);
    ~HidBridge() override;

    QVariantMap requestDevices(const QString &optionsJson, const QString &origin);
    QVariantMap getDevices(const QString &origin);
    QVariantMap openDevice(const QString &deviceId, const QString &origin);
    QVariantMap closeDevice(const QString &deviceId, const QString &origin);
    QVariantMap sendReport(const QString &deviceId,
                                       int reportId,
                                       const QVariantList &data,
                                       const QString &origin);
    QVariantMap sendFeatureReport(const QString &deviceId,
                                              int reportId,
                                              const QVariantList &data,
                                              const QString &origin);
    QVariantMap receiveFeatureReport(const QString &deviceId,
                                                 int reportId,
                                                 int length,
                                                 const QString &origin);
    QVariantMap forgetDevice(const QString &deviceId, const QString &origin);

    QObject *createPageBridge(QWebEnginePage *page);
    void showDeviceManager(QWidget *parent = nullptr);

signals:
    void inputReport(const QString &deviceId, int reportId, const QVariantList &data);
    void deviceDisconnected(const QString &deviceId);

private:
    struct DeviceDescriptor {
        QString id;
        QByteArray path;
        quint16 vendorId = 0;
        quint16 productId = 0;
        QString productName;
        QString serialNumber;
        QString manufacturerName;
        quint16 usagePage = 0;
        quint16 usage = 0;
        int interfaceNumber = -1;
    };

    struct OpenDevice {
        DeviceDescriptor descriptor;
        hid_device *handle = nullptr;
    };

    QList<DeviceDescriptor> enumerateDevices() const;
    DeviceDescriptor descriptorForId(const QString &deviceId) const;
    QVariantMap descriptorToMap(const DeviceDescriptor &device) const;
    QVariantMap success(const QVariant &value = QVariant()) const;
    QVariantMap failure(const QString &error, const QString &name = QStringLiteral("OperationError")) const;
    bool originIsAllowed(const QString &origin) const;
    bool originHasGrant(const QString &origin, const QString &deviceId) const;
    void addGrant(const QString &origin, const QString &deviceId);
    void removeGrant(const QString &origin, const QString &deviceId);
    QStringList grantsForOrigin(const QString &origin) const;
    QString grantsSettingsKey(const QString &origin) const;
    bool matchesOptions(const DeviceDescriptor &device, const QString &optionsJson) const;
    bool matchesFilter(const DeviceDescriptor &device, const QVariantMap &filter) const;
    QString deviceLabel(const DeviceDescriptor &device) const;
    QString hidError(hid_device *handle) const;
    OpenDevice *openedFor(const QString &deviceId, const QString &origin);
    void pollInputReports();
    void closeAllDevices();

    QWidget *m_dialogParent = nullptr;
    QHash<QString, OpenDevice> m_openDevices;
    QTimer *m_pollTimer = nullptr;
};
