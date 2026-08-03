#include "hidbridge.h"

#include <QAbstractItemView>
#include <QCryptographicHash>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEnginePage>

#include <algorithm>
#include <utility>

namespace {
QString wideString(const wchar_t *value)
{
    return value ? QString::fromWCharArray(value) : QString();
}

QString hexId(quint16 value)
{
    return QStringLiteral("0x%1").arg(value, 4, 16, QLatin1Char('0')).toUpper();
}

QByteArray bytesFromVariantList(const QVariantList &values)
{
    QByteArray bytes;
    bytes.reserve(values.size());
    for (const QVariant &value : values)
        bytes.append(static_cast<char>(std::clamp(value.toInt(), 0, 255)));
    return bytes;
}

QVariantList variantListFromBytes(const unsigned char *data, int length)
{
    QVariantList result;
    result.reserve(qMax(0, length));
    for (int index = 0; index < length; ++index)
        result.append(static_cast<int>(data[index]));
    return result;
}

bool isProtectedInputUsage(quint16 usagePage, quint16 usage)
{
    // Do not expose ordinary keyboard or pointer interfaces. Device configurators
    // normally use a separate vendor-defined HID interface (usage page 0xFF00+).
    if (usagePage != 0x0001)
        return false;
    return usage == 0x0001 || usage == 0x0002 || usage == 0x0006 || usage == 0x0007;
}
}

HidBridge::HidBridge(QWidget *dialogParent, QObject *parent)
    : QObject(parent), m_dialogParent(dialogParent), m_pollTimer(new QTimer(this))
{
    hid_init();
    m_pollTimer->setInterval(12);
    connect(m_pollTimer, &QTimer::timeout, this, &HidBridge::pollInputReports);
}

HidBridge::~HidBridge()
{
    closeAllDevices();
    hid_exit();
}

QVariantMap HidBridge::requestDevices(const QString &optionsJson, const QString &origin)
{
    if (!originIsAllowed(origin))
        return failure(QStringLiteral("Device access requires an HTTPS website or localhost."),
                       QStringLiteral("SecurityError"));

    QList<DeviceDescriptor> candidates;
    for (const DeviceDescriptor &device : enumerateDevices()) {
        if (!isProtectedInputUsage(device.usagePage, device.usage)
            && matchesOptions(device, optionsJson))
            candidates.append(device);
    }

    if (candidates.isEmpty())
        return failure(QStringLiteral("No matching HID devices were detected."),
                       QStringLiteral("NotFoundError"));

    QDialog dialog(m_dialogParent);
    dialog.setWindowTitle(QStringLiteral("Connect a device"));
    dialog.resize(590, 430);
    auto *layout = new QVBoxLayout(&dialog);

    auto *heading = new QLabel(
        QStringLiteral("<b>%1</b> wants to connect to a keyboard, mouse, controller, or other HID device. "
                       "Choose only a device you trust.")
            .arg(QUrl(origin).host().toHtmlEscaped()),
        &dialog);
    heading->setWordWrap(true);
    layout->addWidget(heading);

    auto *list = new QListWidget(&dialog);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    for (const DeviceDescriptor &device : std::as_const(candidates)) {
        auto *item = new QListWidgetItem(deviceLabel(device), list);
        item->setData(Qt::UserRole, device.id);
        item->setToolTip(QStringLiteral("Vendor %1 · Product %2 · Interface %3")
                             .arg(hexId(device.vendorId), hexId(device.productId))
                             .arg(device.interfaceNumber));
    }
    if (list->count() > 0)
        list->setCurrentRow(0);
    layout->addWidget(list, 1);

    auto *note = new QLabel(
        QStringLiteral("Access is remembered for this website. You can remove it from Browser menu → Connected Devices."),
        &dialog);
    note->setWordWrap(true);
    layout->addWidget(note);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
    auto *connectButton = buttons->addButton(QStringLiteral("Connect"), QDialogButtonBox::AcceptRole);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(connectButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(list, &QListWidget::itemDoubleClicked, &dialog, [&dialog](QListWidgetItem *) {
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted || !list->currentItem())
        return failure(QStringLiteral("No device was selected."), QStringLiteral("NotFoundError"));

    const QString id = list->currentItem()->data(Qt::UserRole).toString();
    const auto selected = std::find_if(candidates.cbegin(), candidates.cend(),
                                       [&id](const DeviceDescriptor &device) { return device.id == id; });
    if (selected == candidates.cend())
        return failure(QStringLiteral("The selected device is no longer available."),
                       QStringLiteral("NotFoundError"));

    addGrant(origin, selected->id);
    QVariantList devices;
    devices.append(descriptorToMap(*selected));
    return success(devices);
}

QVariantMap HidBridge::getDevices(const QString &origin)
{
    if (!originIsAllowed(origin))
        return failure(QStringLiteral("Device access requires a secure website."),
                       QStringLiteral("SecurityError"));

    const QStringList grants = grantsForOrigin(origin);
    QVariantList devices;
    for (const DeviceDescriptor &device : enumerateDevices()) {
        if (!isProtectedInputUsage(device.usagePage, device.usage) && grants.contains(device.id))
            devices.append(descriptorToMap(device));
    }
    return success(devices);
}

QVariantMap HidBridge::openDevice(const QString &deviceId, const QString &origin)
{
    if (!originHasGrant(origin, deviceId))
        return failure(QStringLiteral("This website has not been granted access to that device."),
                       QStringLiteral("SecurityError"));

    auto existing = m_openDevices.find(deviceId);
    if (existing != m_openDevices.end() && existing->handle)
        return success(true);

    const DeviceDescriptor descriptor = descriptorForId(deviceId);
    if (descriptor.id.isEmpty())
        return failure(QStringLiteral("The device is disconnected."), QStringLiteral("NotFoundError"));

    hid_device *handle = hid_open_path(descriptor.path.constData());
    if (!handle) {
        return failure(
            QStringLiteral("Cachy Surf could not open the device. On Linux this usually means hidraw permission is missing. "
                           "Open Browser menu → Connected Devices → Install Linux device access, then unplug and reconnect the device."),
            QStringLiteral("NotAllowedError"));
    }

    hid_set_nonblocking(handle, 1);
    OpenDevice open;
    open.descriptor = descriptor;
    open.handle = handle;
    m_openDevices.insert(deviceId, open);
    if (!m_pollTimer->isActive())
        m_pollTimer->start();
    return success(true);
}

QVariantMap HidBridge::closeDevice(const QString &deviceId, const QString &origin)
{
    if (!originHasGrant(origin, deviceId))
        return failure(QStringLiteral("This website has no permission for that device."),
                       QStringLiteral("SecurityError"));

    auto iterator = m_openDevices.find(deviceId);
    if (iterator != m_openDevices.end()) {
        if (iterator->handle)
            hid_close(iterator->handle);
        m_openDevices.erase(iterator);
    }
    if (m_openDevices.isEmpty())
        m_pollTimer->stop();
    return success(true);
}

QVariantMap HidBridge::sendReport(const QString &deviceId,
                                  int reportId,
                                  const QVariantList &data,
                                  const QString &origin)
{
    OpenDevice *device = openedFor(deviceId, origin);
    if (!device)
        return failure(QStringLiteral("Open the device before sending a report."),
                       QStringLiteral("InvalidStateError"));

    QByteArray report;
    report.reserve(data.size() + 1);
    report.append(static_cast<char>(std::clamp(reportId, 0, 255)));
    report.append(bytesFromVariantList(data));
    const int result = hid_write(device->handle,
                                 reinterpret_cast<const unsigned char *>(report.constData()),
                                 static_cast<size_t>(report.size()));
    if (result < 0)
        return failure(hidError(device->handle));
    return success(result);
}

QVariantMap HidBridge::sendFeatureReport(const QString &deviceId,
                                         int reportId,
                                         const QVariantList &data,
                                         const QString &origin)
{
    OpenDevice *device = openedFor(deviceId, origin);
    if (!device)
        return failure(QStringLiteral("Open the device before sending a feature report."),
                       QStringLiteral("InvalidStateError"));

    QByteArray report;
    report.reserve(data.size() + 1);
    report.append(static_cast<char>(std::clamp(reportId, 0, 255)));
    report.append(bytesFromVariantList(data));
    const int result = hid_send_feature_report(
        device->handle,
        reinterpret_cast<const unsigned char *>(report.constData()),
        static_cast<size_t>(report.size()));
    if (result < 0)
        return failure(hidError(device->handle));
    return success(result);
}

QVariantMap HidBridge::receiveFeatureReport(const QString &deviceId,
                                            int reportId,
                                            int length,
                                            const QString &origin)
{
    OpenDevice *device = openedFor(deviceId, origin);
    if (!device)
        return failure(QStringLiteral("Open the device before receiving a feature report."),
                       QStringLiteral("InvalidStateError"));

    const int safeLength = std::clamp(length, 1, 65535);
    QByteArray buffer(safeLength + 1, '\0');
    buffer[0] = static_cast<char>(std::clamp(reportId, 0, 255));
    const int result = hid_get_feature_report(
        device->handle,
        reinterpret_cast<unsigned char *>(buffer.data()),
        static_cast<size_t>(buffer.size()));
    if (result < 0)
        return failure(hidError(device->handle));

    const int dataOffset = result > 0 ? 1 : 0;
    return success(variantListFromBytes(
        reinterpret_cast<const unsigned char *>(buffer.constData()) + dataOffset,
        qMax(0, result - dataOffset)));
}

QVariantMap HidBridge::forgetDevice(const QString &deviceId, const QString &origin)
{
    closeDevice(deviceId, origin);
    removeGrant(origin, deviceId);
    return success(true);
}

QObject *HidBridge::createPageBridge(QWebEnginePage *page)
{
    return new HidPageBridge(this, page, page);
}

HidPageBridge::HidPageBridge(HidBridge *bridge, QWebEnginePage *page, QObject *parent)
    : QObject(parent), m_bridge(bridge), m_page(page)
{
    connect(m_bridge, &HidBridge::inputReport, this, &HidPageBridge::inputReport);
    connect(m_bridge, &HidBridge::deviceDisconnected, this, &HidPageBridge::deviceDisconnected);
}

QString HidPageBridge::currentOrigin() const
{
    if (!m_page)
        return {};
    const QUrl url = m_page->url();
    if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty())
        return {};
    QUrl origin;
    origin.setScheme(url.scheme().toLower());
    origin.setHost(url.host().toLower());
    if (url.port() >= 0)
        origin.setPort(url.port());
    return origin.toString(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment | QUrl::StripTrailingSlash);
}

QVariantMap HidPageBridge::requestDevices(const QString &optionsJson)
{
    return m_bridge->requestDevices(optionsJson, currentOrigin());
}

QVariantMap HidPageBridge::getDevices()
{
    return m_bridge->getDevices(currentOrigin());
}

QVariantMap HidPageBridge::openDevice(const QString &deviceId)
{
    return m_bridge->openDevice(deviceId, currentOrigin());
}

QVariantMap HidPageBridge::closeDevice(const QString &deviceId)
{
    return m_bridge->closeDevice(deviceId, currentOrigin());
}

QVariantMap HidPageBridge::sendReport(const QString &deviceId, int reportId, const QVariantList &data)
{
    return m_bridge->sendReport(deviceId, reportId, data, currentOrigin());
}

QVariantMap HidPageBridge::sendFeatureReport(const QString &deviceId, int reportId, const QVariantList &data)
{
    return m_bridge->sendFeatureReport(deviceId, reportId, data, currentOrigin());
}

QVariantMap HidPageBridge::receiveFeatureReport(const QString &deviceId, int reportId, int length)
{
    return m_bridge->receiveFeatureReport(deviceId, reportId, length, currentOrigin());
}

QVariantMap HidPageBridge::forgetDevice(const QString &deviceId)
{
    return m_bridge->forgetDevice(deviceId, currentOrigin());
}

void HidBridge::showDeviceManager(QWidget *parent)
{
    QDialog dialog(parent ? parent : m_dialogParent);
    dialog.setWindowTitle(QStringLiteral("Connected Devices"));
    dialog.resize(680, 470);
    auto *layout = new QVBoxLayout(&dialog);

    auto *description = new QLabel(
        QStringLiteral("Cachy Surf includes an experimental native WebHID bridge for keyboard, mouse, controller, "
                       "and device-configuration websites. A website must ask before it receives access."),
        &dialog);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *list = new QListWidget(&dialog);
    const QList<DeviceDescriptor> devices = enumerateDevices();
    for (const DeviceDescriptor &device : devices) {
        auto *item = new QListWidgetItem(deviceLabel(device), list);
        item->setToolTip(QStringLiteral("Vendor %1 · Product %2 · Usage page %3 · Usage %4 · Interface %5")
                             .arg(hexId(device.vendorId), hexId(device.productId), hexId(device.usagePage),
                                  hexId(device.usage))
                             .arg(device.interfaceNumber));
    }
    if (devices.isEmpty())
        list->addItem(QStringLiteral("No HID devices were detected."));
    layout->addWidget(list, 1);

    auto *linuxNote = new QLabel(
        QStringLiteral("If a website sees the device but cannot open it, Linux hidraw permissions are probably blocking access. "
                       "The button below installs a systemd-logind uaccess rule for active local users. Unplug and reconnect the device afterward."),
        &dialog);
    linuxNote->setWordWrap(true);
    layout->addWidget(linuxNote);

    auto *buttons = new QDialogButtonBox(&dialog);
    auto *permissionsButton = buttons->addButton(QStringLiteral("Install Linux device access…"),
                                                  QDialogButtonBox::ActionRole);
    auto *clearButton = buttons->addButton(QStringLiteral("Clear remembered permissions"),
                                            QDialogButtonBox::DestructiveRole);
    buttons->addButton(QDialogButtonBox::Close);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    connect(clearButton, &QPushButton::clicked, &dialog, [&dialog] {
        QSettings settings;
        settings.remove(QStringLiteral("devices/grants"));
        QMessageBox::information(&dialog, QStringLiteral("Permissions cleared"),
                                 QStringLiteral("Websites will have to ask again before accessing devices."));
    });

    connect(permissionsButton, &QPushButton::clicked, &dialog, [&dialog] {
        const QString helper = QStringLiteral("/usr/lib/cachysurf/install-webhid-access.sh");
        if (!QFileInfo::exists(helper)) {
            QMessageBox::warning(&dialog, QStringLiteral("Helper missing"),
                                 QStringLiteral("The device-access helper was not installed. Reinstall Cachy Surf v0.4 or newer."));
            return;
        }
        const auto answer = QMessageBox::question(
            &dialog,
            QStringLiteral("Install device access"),
            QStringLiteral("This installs a udev rule that gives the currently active local user access to hidraw devices. "
                           "Only continue on a computer you control."),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
        if (!QProcess::startDetached(QStringLiteral("pkexec"), {helper})) {
            QMessageBox::warning(&dialog, QStringLiteral("Could not start installer"),
                                 QStringLiteral("Make sure polkit is installed, or run the helper as root manually:\n%1").arg(helper));
        }
    });

    dialog.exec();
}

QList<HidBridge::DeviceDescriptor> HidBridge::enumerateDevices() const
{
    QList<DeviceDescriptor> devices;
    hid_device_info *head = hid_enumerate(0, 0);
    for (hid_device_info *current = head; current; current = current->next) {
        if (!current->path)
            continue;
        DeviceDescriptor device;
        device.path = QByteArray(current->path);
        device.vendorId = current->vendor_id;
        device.productId = current->product_id;
        device.productName = wideString(current->product_string);
        device.serialNumber = wideString(current->serial_number);
        device.manufacturerName = wideString(current->manufacturer_string);
        device.usagePage = current->usage_page;
        device.usage = current->usage;
        device.interfaceNumber = current->interface_number;

        QByteArray identity = QByteArray::number(device.vendorId) + ':'
            + QByteArray::number(device.productId) + ':'
            + device.serialNumber.toUtf8() + ':'
            + QByteArray::number(device.interfaceNumber) + ':'
            + QByteArray::number(device.usagePage) + ':'
            + QByteArray::number(device.usage);
        if (device.serialNumber.isEmpty())
            identity += ':' + device.path;
        device.id = QString::fromLatin1(
            QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex().left(24));
        devices.append(device);
    }
    hid_free_enumeration(head);
    return devices;
}

HidBridge::DeviceDescriptor HidBridge::descriptorForId(const QString &deviceId) const
{
    for (const DeviceDescriptor &device : enumerateDevices()) {
        if (device.id == deviceId)
            return device;
    }
    return {};
}

QVariantMap HidBridge::descriptorToMap(const DeviceDescriptor &device) const
{
    QVariantMap collection;
    collection.insert(QStringLiteral("usagePage"), device.usagePage);
    collection.insert(QStringLiteral("usage"), device.usage);
    collection.insert(QStringLiteral("inputReports"), QVariantList());
    collection.insert(QStringLiteral("outputReports"), QVariantList());
    collection.insert(QStringLiteral("featureReports"), QVariantList());

    QVariantMap result;
    result.insert(QStringLiteral("id"), device.id);
    result.insert(QStringLiteral("vendorId"), device.vendorId);
    result.insert(QStringLiteral("productId"), device.productId);
    result.insert(QStringLiteral("productName"), device.productName);
    result.insert(QStringLiteral("serialNumber"), device.serialNumber);
    result.insert(QStringLiteral("manufacturerName"), device.manufacturerName);
    result.insert(QStringLiteral("usagePage"), device.usagePage);
    result.insert(QStringLiteral("usage"), device.usage);
    result.insert(QStringLiteral("collections"), QVariantList{collection});
    return result;
}

QVariantMap HidBridge::success(const QVariant &value) const
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("value"), value);
    return result;
}

QVariantMap HidBridge::failure(const QString &error, const QString &name) const
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("error"), error);
    result.insert(QStringLiteral("name"), name);
    return result;
}

bool HidBridge::originIsAllowed(const QString &origin) const
{
    const QUrl url(origin);
    if (!url.isValid())
        return false;
    if (url.scheme() == QStringLiteral("https"))
        return true;
    const QString host = url.host().toLower();
    return url.scheme() == QStringLiteral("http")
        && (host == QStringLiteral("localhost") || host == QStringLiteral("127.0.0.1")
            || host == QStringLiteral("::1"));
}

bool HidBridge::originHasGrant(const QString &origin, const QString &deviceId) const
{
    return originIsAllowed(origin) && grantsForOrigin(origin).contains(deviceId);
}

void HidBridge::addGrant(const QString &origin, const QString &deviceId)
{
    QStringList grants = grantsForOrigin(origin);
    if (!grants.contains(deviceId))
        grants.append(deviceId);
    QSettings().setValue(grantsSettingsKey(origin), grants);
}

void HidBridge::removeGrant(const QString &origin, const QString &deviceId)
{
    QStringList grants = grantsForOrigin(origin);
    grants.removeAll(deviceId);
    QSettings().setValue(grantsSettingsKey(origin), grants);
}

QStringList HidBridge::grantsForOrigin(const QString &origin) const
{
    return QSettings().value(grantsSettingsKey(origin)).toStringList();
}

QString HidBridge::grantsSettingsKey(const QString &origin) const
{
    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash(origin.toUtf8(), QCryptographicHash::Sha256).toHex());
    return QStringLiteral("devices/grants/") + hash;
}

bool HidBridge::matchesOptions(const DeviceDescriptor &device, const QString &optionsJson) const
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(optionsJson.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return true;

    const QJsonObject object = document.object();
    const QJsonArray exclusions = object.value(QStringLiteral("exclusionFilters")).toArray();
    for (const QJsonValue &value : exclusions) {
        if (value.isObject() && matchesFilter(device, value.toObject().toVariantMap()))
            return false;
    }

    const QJsonArray filters = object.value(QStringLiteral("filters")).toArray();
    if (filters.isEmpty())
        return true;
    for (const QJsonValue &value : filters) {
        if (value.isObject() && matchesFilter(device, value.toObject().toVariantMap()))
            return true;
    }
    return false;
}

bool HidBridge::matchesFilter(const DeviceDescriptor &device, const QVariantMap &filter) const
{
    const auto matchesNumber = [&filter](const QString &key, quint16 actual) {
        return !filter.contains(key) || filter.value(key).toUInt() == actual;
    };
    return matchesNumber(QStringLiteral("vendorId"), device.vendorId)
        && matchesNumber(QStringLiteral("productId"), device.productId)
        && matchesNumber(QStringLiteral("usagePage"), device.usagePage)
        && matchesNumber(QStringLiteral("usage"), device.usage);
}

QString HidBridge::deviceLabel(const DeviceDescriptor &device) const
{
    QString name = device.productName.trimmed();
    if (name.isEmpty())
        name = QStringLiteral("HID device");
    QString manufacturer = device.manufacturerName.trimmed();
    if (!manufacturer.isEmpty() && !name.contains(manufacturer, Qt::CaseInsensitive))
        name = manufacturer + QStringLiteral(" ") + name;
    return QStringLiteral("%1\n%2:%3 · Interface %4")
        .arg(name, hexId(device.vendorId), hexId(device.productId))
        .arg(device.interfaceNumber);
}

QString HidBridge::hidError(hid_device *handle) const
{
    const QString error = wideString(hid_error(handle));
    return error.isEmpty() ? QStringLiteral("The HID operation failed.") : error;
}

HidBridge::OpenDevice *HidBridge::openedFor(const QString &deviceId, const QString &origin)
{
    if (!originHasGrant(origin, deviceId))
        return nullptr;
    auto iterator = m_openDevices.find(deviceId);
    if (iterator == m_openDevices.end() || !iterator->handle)
        return nullptr;
    return &iterator.value();
}

void HidBridge::pollInputReports()
{
    QList<QString> disconnected;
    for (auto iterator = m_openDevices.begin(); iterator != m_openDevices.end(); ++iterator) {
        unsigned char buffer[65536];
        int result = 0;
        do {
            result = hid_read_timeout(iterator->handle, buffer, sizeof(buffer), 0);
            if (result > 0) {
                const int reportId = buffer[0];
                const int offset = result > 1 ? 1 : 0;
                emit inputReport(iterator.key(), reportId,
                                 variantListFromBytes(buffer + offset, result - offset));
            } else if (result < 0) {
                disconnected.append(iterator.key());
            }
        } while (result > 0);
    }

    for (const QString &id : std::as_const(disconnected)) {
        auto iterator = m_openDevices.find(id);
        if (iterator == m_openDevices.end())
            continue;
        if (iterator->handle)
            hid_close(iterator->handle);
        m_openDevices.erase(iterator);
        emit deviceDisconnected(id);
    }
    if (m_openDevices.isEmpty())
        m_pollTimer->stop();
}

void HidBridge::closeAllDevices()
{
    m_pollTimer->stop();
    for (OpenDevice &device : m_openDevices) {
        if (device.handle)
            hid_close(device.handle);
    }
    m_openDevices.clear();
}
