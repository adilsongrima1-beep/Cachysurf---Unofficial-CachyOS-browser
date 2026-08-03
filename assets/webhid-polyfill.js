(() => {
    'use strict';
    if (window.__cachySurfWebHidInstalled)
        return;
    window.__cachySurfWebHidInstalled = true;

    const bridgeReady = new Promise((resolve, reject) => {
        let attempts = 0;
        const connect = () => {
            if (typeof QWebChannel !== 'undefined' && window.qt?.webChannelTransport) {
                new QWebChannel(window.qt.webChannelTransport, channel => {
                    const bridge = channel.objects.cachyHid;
                    if (bridge)
                        resolve(bridge);
                    else
                        reject(new Error('Cachy Surf HID bridge is unavailable.'));
                });
                return;
            }
            if (++attempts > 500) {
                reject(new Error('Cachy Surf HID bridge did not initialize.'));
                return;
            }
            setTimeout(connect, 10);
        };
        connect();
    });

    const call = async (name, ...args) => {
        const bridge = await bridgeReady;
        const result = await new Promise(resolve => bridge[name](...args, resolve));
        if (!result?.ok)
            throw new DOMException(result?.error || 'The device operation failed.', result?.name || 'OperationError');
        return result.value;
    };

    const bytes = value => {
        if (value instanceof DataView)
            return Array.from(new Uint8Array(value.buffer, value.byteOffset, value.byteLength));
        if (ArrayBuffer.isView(value))
            return Array.from(new Uint8Array(value.buffer, value.byteOffset, value.byteLength));
        if (value instanceof ArrayBuffer)
            return Array.from(new Uint8Array(value));
        if (Array.isArray(value))
            return value.map(number => Number(number) & 255);
        throw new TypeError('Expected an ArrayBuffer, DataView, or typed array.');
    };

    const deviceCache = new Map();

    class CachyHIDInputReportEvent extends Event {
        constructor(type, init) {
            super(type);
            this.device = init.device;
            this.reportId = init.reportId;
            this.data = init.data;
        }
    }

    class CachyHIDConnectionEvent extends Event {
        constructor(type, init) {
            super(type);
            this.device = init.device;
        }
    }

    class CachyHIDDevice extends EventTarget {
        constructor(info) {
            super();
            this.__id = info.id;
            this.vendorId = Number(info.vendorId || 0);
            this.productId = Number(info.productId || 0);
            this.productName = String(info.productName || 'HID device');
            this.serialNumber = String(info.serialNumber || '');
            this.collections = Array.isArray(info.collections) ? info.collections : [];
            this.opened = false;
            this.oninputreport = null;
        }

        async open() {
            await call('openDevice', this.__id);
            this.opened = true;
        }

        async close() {
            await call('closeDevice', this.__id);
            this.opened = false;
        }

        async forget() {
            await call('forgetDevice', this.__id);
            this.opened = false;
            deviceCache.delete(this.__id);
        }

        async sendReport(reportId, data) {
            if (!this.opened)
                throw new DOMException('The device is not open.', 'InvalidStateError');
            await call('sendReport', this.__id, Number(reportId), bytes(data));
        }

        async sendFeatureReport(reportId, data) {
            if (!this.opened)
                throw new DOMException('The device is not open.', 'InvalidStateError');
            await call('sendFeatureReport', this.__id, Number(reportId), bytes(data));
        }

        async receiveFeatureReport(reportId) {
            if (!this.opened)
                throw new DOMException('The device is not open.', 'InvalidStateError');
            const result = await call('receiveFeatureReport', this.__id, Number(reportId), 65535);
            const array = Uint8Array.from(result || []);
            return new DataView(array.buffer);
        }

        __dispatchInput(reportId, reportBytes) {
            const array = Uint8Array.from(reportBytes || []);
            const event = new CachyHIDInputReportEvent('inputreport', {
                device: this,
                reportId: Number(reportId || 0),
                data: new DataView(array.buffer)
            });
            this.dispatchEvent(event);
            if (typeof this.oninputreport === 'function')
                this.oninputreport.call(this, event);
        }
    }

    const fromInfo = info => {
        if (!info?.id)
            return null;
        let device = deviceCache.get(info.id);
        if (!device) {
            device = new CachyHIDDevice(info);
            deviceCache.set(info.id, device);
        }
        return device;
    };

    class CachyHID extends EventTarget {
        constructor() {
            super();
            this.onconnect = null;
            this.ondisconnect = null;
            bridgeReady.then(bridge => {
                bridge.inputReport.connect((id, reportId, data) => {
                    const device = deviceCache.get(id);
                    if (device)
                        device.__dispatchInput(reportId, data);
                });
                bridge.deviceDisconnected.connect(id => {
                    const device = deviceCache.get(id);
                    if (!device)
                        return;
                    device.opened = false;
                    const event = new CachyHIDConnectionEvent('disconnect', { device });
                    this.dispatchEvent(event);
                    if (typeof this.ondisconnect === 'function')
                        this.ondisconnect.call(this, event);
                });
            }).catch(() => {});
        }

        async getDevices() {
            const infos = await call('getDevices');
            return (infos || []).map(fromInfo).filter(Boolean);
        }

        async requestDevice(options = {}) {
            const safeOptions = {
                filters: Array.isArray(options.filters) ? options.filters : [],
                exclusionFilters: Array.isArray(options.exclusionFilters) ? options.exclusionFilters : []
            };
            const infos = await call('requestDevices', JSON.stringify(safeOptions));
            return (infos || []).map(fromInfo).filter(Boolean);
        }
    }

    const hid = new CachyHID();
    let installed = false;
    try {
        Object.defineProperty(navigator, 'hid', {
            configurable: true,
            enumerable: true,
            get: () => hid
        });
        installed = navigator.hid === hid;
    } catch (_) {}

    if (!installed) {
        try {
            Object.defineProperty(Navigator.prototype, 'hid', {
                configurable: true,
                enumerable: true,
                get: () => hid
            });
        } catch (_) {}
    }

    if (!window.HIDDevice)
        window.HIDDevice = CachyHIDDevice;
    if (!window.HIDInputReportEvent)
        window.HIDInputReportEvent = CachyHIDInputReportEvent;
    if (!window.HIDConnectionEvent)
        window.HIDConnectionEvent = CachyHIDConnectionEvent;
})();
