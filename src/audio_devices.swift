// Kotib — mikrofon qurilmalarini aniqlash (CoreAudio HAL).
//
// Nima uchun AVCaptureDevice emas: bizga qurilmaning transport turi (Bluetooth,
// virtual, aggregate) va tizim standarti qaysiligi kerak. Bu maʼlumotlarni HAL
// toʻgʻridan-toʻgʻri beradi, AVFoundation esa yoʻq yoki chala beradi.
//
// Windows'dagi settings_window.cpp bilan bir xil ogohlantirishlar chiqadi.

import Foundation
import CoreAudio

/// Sozlamalar roʻyxatidagi bitta mikrofon.
struct MicDevice {
    let uid: String  // CoreAudio UID — barqaror, qayta ulanganda ham oʻzgarmaydi
    let name: String
    let kind: Kind
    let isDefault: Bool

    enum Kind {
        case builtIn, usb, bluetooth, virtual, aggregate, other

        /// Roʻyxatdagi qisqa yorliq. Windows'dagi matnlar bilan bir xil.
        var badge: String {
            switch self {
            case .virtual: return "   [mikrofon emas!]"
            case .bluetooth: return "   [sifat past]"
            case .aggregate: return "   [virtual]"
            default: return ""
            }
        }

        /// Roʻyxat ostida chiqadigan toʻliq ogohlantirish. Boʻsh — muammo yoʻq.
        var warning: String {
            switch self {
            case .virtual:
                return "Bu haqiqiy mikrofon emas — virtual qurilma (masalan BlackHole). "
                    + "U tizim ovozini uzatadi, gapirganingizni eshitmaydi."
            case .bluetooth:
                return "Bluetooth mikrofon: ovoz sifati past boʻladi va matn xato chiqishi mumkin. "
                    + "Iloji boʻlsa ichki yoki simli mikrofonni tanlang."
            case .aggregate:
                return "Birlashtirilgan (aggregate) qurilma — kutilmagan natija berishi mumkin."
            default:
                return ""
            }
        }
    }

    /// Roʻyxatda koʻrinadigan toʻliq qator.
    var listLabel: String {
        name + kind.badge + (isDefault ? "   (standart)" : "")
    }
}

enum AudioDevices {

    /// Kirish (mikrofon) qurilmalari. Kirish kanali yoʻq qurilmalar tashlab yuboriladi.
    static func inputDevices() -> [MicDevice] {
        let defUID = defaultInputUID()
        return allDeviceIDs().compactMap { id in
            guard inputChannelCount(id) > 0 else { return nil }
            guard let uid = stringProperty(id, kAudioDevicePropertyDeviceUID) else { return nil }
            let name = stringProperty(id, kAudioObjectPropertyName) ?? uid
            return MicDevice(uid: uid, name: name, kind: kind(of: id), isDefault: uid == defUID)
        }
    }

    /// UID boʻyicha qurilma ID — AVAudioEngine'ga qaysi mikrofonni berishni aytish uchun.
    /// Qurilma uzilgan boʻlsa nil qaytadi (chaqiruvchi standartga qaytadi).
    static func deviceID(forUID uid: String) -> AudioDeviceID? {
        allDeviceIDs().first { stringProperty($0, kAudioDevicePropertyDeviceUID) == uid }
    }

    /// Tizim standart mikrofoni haqida qisqa maʼlumot (sozlamalarda koʻrsatiladi).
    static func defaultInput() -> MicDevice? {
        guard let uid = defaultInputUID() else { return nil }
        return inputDevices().first { $0.uid == uid }
    }

    // MARK: - Ichki yordamchilar

    private static func defaultInputUID() -> String? {
        var addr = AudioObjectPropertyAddress(
            mSelector: kAudioHardwarePropertyDefaultInputDevice,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMain)
        var id = AudioDeviceID(0)
        var size = UInt32(MemoryLayout<AudioDeviceID>.size)
        let st = AudioObjectGetPropertyData(
            AudioObjectID(kAudioObjectSystemObject),
            &addr, 0, nil, &size, &id)
        guard st == noErr, id != 0 else { return nil }
        return stringProperty(id, kAudioDevicePropertyDeviceUID)
    }

    private static func allDeviceIDs() -> [AudioDeviceID] {
        var addr = AudioObjectPropertyAddress(
            mSelector: kAudioHardwarePropertyDevices,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMain)
        var size: UInt32 = 0
        guard
            AudioObjectGetPropertyDataSize(
                AudioObjectID(kAudioObjectSystemObject),
                &addr, 0, nil, &size) == noErr, size > 0
        else { return [] }
        let count = Int(size) / MemoryLayout<AudioDeviceID>.size
        var ids = [AudioDeviceID](repeating: 0, count: count)
        guard
            AudioObjectGetPropertyData(
                AudioObjectID(kAudioObjectSystemObject),
                &addr, 0, nil, &size, &ids) == noErr
        else { return [] }
        return ids
    }

    /// Kirish kanallari soni. 0 boʻlsa — bu chiquvchi qurilma, roʻyxatga kirmaydi.
    private static func inputChannelCount(_ id: AudioDeviceID) -> Int {
        var addr = AudioObjectPropertyAddress(
            mSelector: kAudioDevicePropertyStreamConfiguration,
            mScope: kAudioObjectPropertyScopeInput,
            mElement: kAudioObjectPropertyElementMain)
        var size: UInt32 = 0
        guard AudioObjectGetPropertyDataSize(id, &addr, 0, nil, &size) == noErr, size > 0 else { return 0 }

        let raw = UnsafeMutableRawPointer.allocate(
            byteCount: Int(size),
            alignment: MemoryLayout<AudioBufferList>.alignment)
        defer { raw.deallocate() }
        guard AudioObjectGetPropertyData(id, &addr, 0, nil, &size, raw) == noErr else { return 0 }

        let listPtr = raw.assumingMemoryBound(to: AudioBufferList.self)
        let buffers = UnsafeMutableAudioBufferListPointer(listPtr)
        return buffers.reduce(0) { $0 + Int($1.mNumberChannels) }
    }

    private static func kind(of id: AudioDeviceID) -> MicDevice.Kind {
        var addr = AudioObjectPropertyAddress(
            mSelector: kAudioDevicePropertyTransportType,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMain)
        var transport: UInt32 = 0
        var size = UInt32(MemoryLayout<UInt32>.size)
        guard AudioObjectGetPropertyData(id, &addr, 0, nil, &size, &transport) == noErr else { return .other }

        switch transport {
        case kAudioDeviceTransportTypeBuiltIn: return .builtIn
        case kAudioDeviceTransportTypeUSB: return .usb
        case kAudioDeviceTransportTypeBluetooth,
            kAudioDeviceTransportTypeBluetoothLE:
            return .bluetooth
        case kAudioDeviceTransportTypeVirtual: return .virtual
        case kAudioDeviceTransportTypeAggregate: return .aggregate
        default: return .other
        }
    }

    private static func stringProperty(_ id: AudioDeviceID, _ selector: AudioObjectPropertySelector) -> String? {
        var addr = AudioObjectPropertyAddress(
            mSelector: selector,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMain)
        var value: CFString = "" as CFString
        var size = UInt32(MemoryLayout<CFString?>.size)
        let st = withUnsafeMutablePointer(to: &value) {
            AudioObjectGetPropertyData(id, &addr, 0, nil, &size, $0)
        }
        guard st == noErr else { return nil }
        return value as String
    }
}
