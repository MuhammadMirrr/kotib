#!/usr/bin/env python3
"""VM ekranini koʻrish va boshqarish — VNC (RFB 3.8) orqali.

NEGA KERAK: UTM'ning `input scan code` API'si kengaytirilgan (0xE0 bilan
boshlanadigan) skan kodlarini — Win, strelkalar, Delete — yubormaydi, va
`screencapture` macOS ekrani qulflangan boʻlsa oyna rasmini ololmaydi.
Ikkalasi ham VM ustida ishlashni deyarli imkonsiz qiladi.

QEMU'ning oʻz VNC serveri ikkala muammoni ham yechadi: kadr buferini
toʻgʻridan-toʻgʻri oʻqiymiz (macOS oynasi umuman kerak emas) va tugmalarni
keysym sifatida yuboramiz — Win tugmasi ham, Ctrl+Shift+Enter ham ishlaydi.

Server UTM konfiguratsiyasidagi `AdditionalArguments` orqali yoqiladi:

    QEMU → AdditionalArguments = ["-vnc", "127.0.0.1:1"]

(Roʻyxat ODDIY satrlardan iborat boʻlishi shart — lugʻat koʻrinishida UTM
butun konfiguratsiyani «yaroqsiz» deb rad etadi.)

Ishlatish:

    vnc.py surat [fayl.png]        ekran rasmini oladi
    vnc.py yoz '<matn>'            matn yozadi (Enter'siz)
    vnc.py qator '<matn>'          matn yozadi va Enter bosadi
    vnc.py tugma super+r ...       tugma/kombinatsiya bosadi
    vnc.py klik <x> <y> [left|right]  sichqoncha bilan bosadi (UTM orqali)
    vnc.py sudra <x1> <y1> <x2> <y2>  sudrab tashlaydi (drag & drop)
    vnc.py olcham                  ekran oʻlchamini aytadi
"""

import os
import socket
import struct
import subprocess
import sys
import time
import zlib

MANZIL = ("127.0.0.1", 5901)

# Sichqoncha UTM'ning oʻz API'si orqali yuboriladi, VNC orqali EMAS.
#
# QEMU'da usb-tablet (absolyut) va usb-mouse (nisbiy) ikkalasi ham ulangan;
# qaysi biri oxirgi soʻralgan boʻlsa, oʻsha faol boʻlib qoladi va VNC
# koordinatalarni nisbiy siljish deb yuboradi. Natijada bosish tasodifiy
# joyga tushadi. UTM'ning `input mouse click` esa SPICE orqali doim
# absolyut ishlaydi.
VM_NOM_PREFIKS = "Kotib-Win11-ARM"

# TUZOQ: sichqoncha bir necha soatlik ishdan keyin butunlay javob bermay
# qoʻyishi mumkin — na UTM, na VNC yoʻli. Klaviatura ishlayveradi va
# mehmondagi qurilmalar «Started» boʻlib turadi, yaʼni ayb QEMU'ning
# kiritish holatida. Mehmonni qayta yuklash YETARLI EMAS; VM'ni butunlay
# toʻxtatib, qaytadan yoqish kerak (QEMU jarayoni yangilanadi):
#
#   osascript -e 'tell application "UTM" to stop virtual machine named "…"'
#   osascript -e 'tell application "UTM" to start virtual machine named "…"' 

# X11 keysym'lar — faqat kerak boʻladiganlari.
TUGMALAR = {
    "enter": 0xFF0D, "return": 0xFF0D, "esc": 0xFF1B, "escape": 0xFF1B,
    "tab": 0xFF09, "backspace": 0xFF08, "delete": 0xFFFF, "space": 0x0020,
    "left": 0xFF51, "up": 0xFF52, "right": 0xFF53, "down": 0xFF54,
    "home": 0xFF50, "end": 0xFF57, "pgup": 0xFF55, "pgdn": 0xFF56,
    "insert": 0xFF63,
    "shift": 0xFFE1, "ctrl": 0xFFE3, "alt": 0xFFE9, "super": 0xFFEB,
    "win": 0xFFEB, "altgr": 0xFFEA,
    **{f"f{i}": 0xFFBD + i for i in range(1, 13)},
}


# AQSh klaviaturasida Shift bilan chiqadigan belgilar → asosiy tugmasi.
USTKI = {
    "~": "`", "!": "1", "@": "2", "#": "3", "$": "4", "%": "5", "^": "6",
    "&": "7", "*": "8", "(": "9", ")": "0", "_": "-", "+": "=", "{": "[",
    "}": "]", "|": "\\", ":": ";", '"': "'", "<": ",", ">": ".", "?": "/",
}


class Vnc:
    def __init__(self, manzil=MANZIL):
        self.s = socket.create_connection(manzil, timeout=15)
        self.s.settimeout(15)
        self._qolgan = b""
        self._salom()

    # ---- past daraja ----

    def _oq(self, n):
        b = bytearray()
        while len(b) < n:
            p = self.s.recv(n - len(b))
            if not p:
                raise RuntimeError("VNC ulanishi uzildi")
            b += p
        return bytes(b)

    def _salom(self):
        versiya = self._oq(12)
        if not versiya.startswith(b"RFB "):
            raise RuntimeError(f"RFB emas: {versiya!r}")
        self.s.sendall(b"RFB 003.008\n")

        n = self._oq(1)[0]
        if n == 0:
            uz = struct.unpack(">I", self._oq(4))[0]
            raise RuntimeError("VNC rad etdi: " + self._oq(uz).decode("utf-8", "replace"))
        turlar = self._oq(n)
        if 1 not in turlar:
            raise RuntimeError(f"«None» autentifikatsiyasi yoʻq: {list(turlar)}")
        self.s.sendall(bytes([1]))
        natija = struct.unpack(">I", self._oq(4))[0]
        if natija != 0:
            uz = struct.unpack(">I", self._oq(4))[0]
            raise RuntimeError("auth: " + self._oq(uz).decode("utf-8", "replace"))

        self.s.sendall(bytes([1]))              # ClientInit: birga foydalanish
        bosh = self._oq(24)
        self.w, self.h = struct.unpack(">HH", bosh[:4])
        nomUz = struct.unpack(">I", bosh[20:24])[0]
        self.nom = self._oq(nomUz).decode("utf-8", "replace")

        # Piksel formati: 32 bit, true-color, B G R X tartibida.
        self.s.sendall(struct.pack(">B3x", 0) + struct.pack(
            ">BBBBHHHBBB3x", 32, 24, 0, 1, 255, 255, 255, 16, 8, 0))
        # Faqat Raw va oʻlcham oʻzgarishi — dekodlash sodda boʻlsin.
        self.s.sendall(struct.pack(">BxH", 2, 2) + struct.pack(">ii", 0, -223))

    # ---- kadr buferi ----

    def kadr(self, urinish=3):
        """Butun ekranni (w, h, RGB baytlar) koʻrinishida qaytaradi."""
        for _ in range(urinish):
            self.s.sendall(struct.pack(">BBHHHH", 3, 0, 0, 0, self.w, self.h))
            piksel = self._kadrniOq()
            if piksel is not None:
                return self.w, self.h, piksel
        raise RuntimeError("kadr kelmadi")

    def _kadrniOq(self):
        while True:
            tur = self._oq(1)[0]
            if tur == 1:                                    # SetColourMapEntries
                _, __, n = struct.unpack(">BHH", self._oq(5))
                self._oq(n * 6)
                continue
            if tur == 2:                                    # Bell
                continue
            if tur == 3:                                    # ServerCutText
                n = struct.unpack(">I", self._oq(7)[3:])[0]
                self._oq(n)
                continue
            if tur != 0:
                raise RuntimeError(f"nomaʼlum RFB xabari: {tur}")

            son = struct.unpack(">xH", self._oq(3))[0]
            buf = bytearray(self.w * self.h * 3)
            olindi = False
            for _ in range(son):
                x, y, w, h, kod = struct.unpack(">HHHHi", self._oq(12))
                if kod == -223:                             # DesktopSize
                    self.w, self.h = w, h
                    return None                             # qaytadan soʻraymiz
                if kod != 0:
                    raise RuntimeError(f"kutilmagan kodlash: {kod}")
                xom = self._oq(w * h * 4)
                for satr in range(h):
                    manba = satr * w * 4
                    nishon = ((y + satr) * self.w + x) * 3
                    for ustun in range(w):
                        b, g, r = xom[manba + ustun * 4: manba + ustun * 4 + 3]
                        buf[nishon + ustun * 3: nishon + ustun * 3 + 3] = bytes((r, g, b))
                olindi = True
            if olindi:
                return bytes(buf)

    def suratQil(self, yol):
        w, h, piksel = self.kadr()
        satrlar = b"".join(b"\x00" + piksel[i * w * 3:(i + 1) * w * 3] for i in range(h))

        def bolak(nom, data):
            b = nom + data
            return struct.pack(">I", len(data)) + b + struct.pack(">I", zlib.crc32(b))

        png = (b"\x89PNG\r\n\x1a\n"
               + bolak(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
               + bolak(b"IDAT", zlib.compress(satrlar, 6))
               + bolak(b"IEND", b""))
        with open(yol, "wb") as f:
            f.write(png)
        return w, h

    def kursorda(self, x, y, qirra=24):
        """Kursor (x, y) atrofidami — ekranning oʻsha boʻlagi oʻzgardimi.

        Ikki kadrni taqqoslash oʻrniga soddaroq yoʻl: kursor turgan joyda
        kamida bitta piksel fon rangidan farq qiladi. Aniq emas, lekin
        «bosish umuman yetib bordimi» degan savolga yetarli.
        """
        w, h, a = self.kadr()
        x0, y0 = max(0, x - 2), max(0, y - 2)
        x1, y1 = min(w, x + qirra), min(h, y + qirra)
        ranglar = set()
        for yy in range(y0, y1):
            bosh = (yy * w + x0) * 3
            for xx in range(x1 - x0):
                ranglar.add(a[bosh + xx * 3: bosh + xx * 3 + 3])
        # Kursor oq va qora chegarali — bir tekis fonda bu ikki rang paydo
        # boʻladi. Rang xilma-xilligi 3 dan koʻp boʻlsa kursor shu yerda.
        return len(ranglar) > 3

    # ---- kiritish ----

    def tugmaHodisa(self, keysym, bosildi):
        self.s.sendall(struct.pack(">BBxxI", 4, 1 if bosildi else 0, keysym))

    def tugmaBos(self, keysym, kutish=0.03):
        self.tugmaHodisa(keysym, True)
        time.sleep(kutish)
        self.tugmaHodisa(keysym, False)
        time.sleep(kutish)

    def kombinatsiya(self, tavsif):
        """`ctrl+shift+enter` kabi tavsifni bosadi."""
        qismlar = tavsif.split("+")
        kodlar = [self._keysym(q) for q in qismlar]
        for k in kodlar:
            self.tugmaHodisa(k, True)
            time.sleep(0.03)
        for k in reversed(kodlar):
            self.tugmaHodisa(k, False)
            time.sleep(0.03)

    @staticmethod
    def _keysym(nom):
        past = nom.lower()
        if past in TUGMALAR:
            return TUGMALAR[past]
        if len(nom) == 1:
            return ord(nom)
        raise SystemExit(f"nomaʼlum tugma: {nom}")

    def matnYoz(self, matn, kutish=0.02):
        for ch in matn:
            asos, shift = self._belgi(ch)
            if shift:
                self.tugmaHodisa(TUGMALAR["shift"], True)
                time.sleep(0.02)
            self.tugmaBos(asos, kutish)
            if shift:
                self.tugmaHodisa(TUGMALAR["shift"], False)
                time.sleep(0.02)

    @staticmethod
    def _belgi(ch):
        """Belgini (asosiy keysym, Shift kerakmi) juftligiga aylantiradi.

        QEMU'ning VNC serveri katta HARFLAR uchun Shift'ni oʻzi qoʻshadi,
        lekin ustki registr BELGILARI uchun qoʻshmaydi: `:` yuborilsa `;`
        chiqadi, `&` yuborilsa `7`. Shuning uchun faqat belgilarni oʻzimiz
        ochib beramiz.
        """
        if ch in USTKI:
            return ord(USTKI[ch]), True
        # Katta harflar uchun Shift'ni QEMU OʻZI qoʻshadi ('A' keysym'i
        # «shift + a» ga oʻgiriladi). Bunga qoʻshimcha Shift yuborilsa
        # aksincha buziladi — kichik harf chiqadi.
        kod = ord(ch)
        # ASCII'dan tashqarisi QEMU'da keysym'ga oʻgirilmaydi — bunday
        # matnni bufer orqali kiritish kerak.
        return (kod if kod <= 0x7E else 0x01000000 + kod), False

    # ---- Sichqonchani sudrash (drag & drop) ----
    #
    # Bosish UTM orqali ketadi (u absolyut), sudrash esa faqat shu yerda
    # mumkin: UTM API'sida tugmani bosib turish yoʻq.
    #
    # VNC koordinatalari QEMU'da NISBIY siljishga aylanadi (usb-tablet va
    # usb-mouse ikkalasi ham ulangan). Shuning uchun avval kursorni chap
    # yuqori burchakka qisamiz — shundan keyin yuborilgan koordinata
    # aynan piksel boʻlib tushadi. Buning ishlashi uchun mehmonda
    # sichqoncha tezlanishi oʻchirilgan boʻlishi SHART
    # (`win/tools/vm-sichqoncha.ps1`).
    def _surish(self, x, y, maska=0):
        self.s.sendall(struct.pack(">BBHH", 5, maska, x, y))
        time.sleep(0.05)

    def uyga(self):
        for _ in range(4):
            self._surish(0, 0)
        time.sleep(0.25)

    def vncKlik(self, x, y, maska=1):
        """Bosish — VNC orqali, kursorni burchakdan hisoblab.

        UTM'ning `input mouse click` API'si SPICE'ga boradi va faqat
        usb-tablet FAOL boʻlganda ishlaydi. Mehmon oxirgi marta usb-mouse'ni
        soʻragan boʻlsa, u jimgina eʼtiborsiz qoladi (AppleScript baribir
        muvaffaqiyat qaytaradi). Bu yoʻl esa har doim ishlaydi.
        """
        self.uyga()
        self._surish(x, y)
        time.sleep(0.2)
        self._surish(x, y, maska)
        time.sleep(0.12)
        self._surish(x, y, 0)
        time.sleep(0.2)

    def sudra(self, x1, y1, x2, y2, qadam=12):
        self.uyga()
        self._surish(x1, y1)
        time.sleep(0.3)
        self._surish(x1, y1, 1)              # tugma bosildi
        time.sleep(0.3)
        for i in range(1, qadam + 1):
            self._surish(x1 + (x2 - x1) * i // qadam,
                         y1 + (y2 - y1) * i // qadam, 1)
        time.sleep(0.4)
        self._surish(x2, y2, 1)
        time.sleep(0.3)
        self._surish(x2, y2, 0)              # qoʻyib yuborildi
        time.sleep(0.3)

    def yop(self):
        try:
            self.s.close()
        except OSError:
            pass


# ---- Sichqoncha (UTM AppleScript) ----

def utm(buyruq):
    return subprocess.run(["osascript", "-e", f'tell application "UTM" to {buyruq}'],
                          capture_output=True, text=True).stdout.strip()


def vmId():
    nom = os.environ.get("VM_NOM")
    if not nom:
        nomlar = [n.strip() for n in utm("get name of every virtual machine").split(",")]
        mos = [n for n in nomlar if n.startswith(VM_NOM_PREFIKS)]
        if not mos:
            raise SystemExit(f"«{VM_NOM_PREFIKS}…» nomli VM topilmadi")
        nom = sorted(mos)[-1]        # eng yangi raqamli
    ident = utm(f'get id of virtual machine named "{nom}"')
    if not ident:
        raise SystemExit(f"VM topilmadi: {nom}")
    return ident


def klik(v, x, y, tugma="left"):
    """Avval UTM orqali (absolyut, tez), ishlamasa VNC orqali.

    UTM yoʻli mehmonda usb-tablet faol boʻlgandagina ishlaydi va bu holat
    oʻz-oʻzidan almashib turadi. Shuning uchun bosishdan keyin kursor
    haqiqatan koʻchganini tekshiramiz.
    """
    qism = "" if tugma == "left" else f" with mouse button {tugma}"
    utm(f'input mouse click (virtual machine id "{vmId()}") at {{{x}, {y}}}{qism}')
    time.sleep(0.35)
    if v.kursorda(x, y):
        return
    v.vncKlik(x, y, 1 if tugma == "left" else 4)


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    buyruq = argv[1]
    v = Vnc()
    try:
        if buyruq == "surat":
            yol = argv[2] if len(argv) > 2 else "/tmp/kotib-vnc.png"
            w, h = v.suratQil(yol)
            print(f"{yol} ({w}x{h})")
        elif buyruq == "olcham":
            print(f"{v.w}x{v.h}  «{v.nom}»")
        elif buyruq == "yoz":
            v.matnYoz(argv[2])
        elif buyruq == "qator":
            v.matnYoz(argv[2])
            time.sleep(0.2)
            v.tugmaBos(TUGMALAR["enter"])
        elif buyruq == "tugma":
            for t in argv[2:]:
                v.kombinatsiya(t)
                time.sleep(0.15)
        elif buyruq == "sudra":
            v.sudra(int(argv[2]), int(argv[3]), int(argv[4]), int(argv[5]))
        elif buyruq == "klik":
            klik(v, int(argv[2]), int(argv[3]), argv[4] if len(argv) > 4 else "left")
        else:
            print(__doc__)
            return 2
    finally:
        v.yop()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
