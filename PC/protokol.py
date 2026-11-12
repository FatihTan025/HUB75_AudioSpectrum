"""Spektrum Analizor haberlesme protokolu (bkz. firmware Protocol.c).

Tasima katmani: UART (pyserial), WiFi (TCP, ESP32 kopru), Bluetooth (BLE NUS, bleak).
Hepsi ayni arayuz: Gonder(bytes) ve gelen baytlar icin Alici geri cagrisi.
"""
import socket, struct, threading, time, queue

SOF = 0xA5
DURUM = {0: 'tamam', 1: 'bilinmeyen komut', 2: 'bilinmeyen parametre', 3: 'aralik disi',
         4: 'CRC hatasi', 5: 'mesgul', 6: 'desteklenmiyor', 7: 'guncelleme hatasi'}
TUR = {0: 'bool', 1: 'int', 2: 'enum', 3: 'renk', 4: 'palet', 5: 'ton', 6: 'blok'}
AILE = {0: 'Spektrum', 1: 'Animasyon', 2: 'Ortam'}
BAYRAK_MOD = 0x01
BAYRAK_SALT_OKUNUR = 0x02
BAYRAK_GIZLI = 0x08
MOD_GENEL = 0xFF
EVT_ACILIS, EVT_MOD, EVT_PARAM, EVT_GUC = 1, 2, 3, 4

NUS_SERVIS = '6e400001-b5a3-f393-e0a9-e50e24dcca9e'
NUS_YAZ = '6e400002-b5a3-f393-e0a9-e50e24dcca9e'		# telefon -> kart
NUS_OKU = '6e400003-b5a3-f393-e0a9-e50e24dcca9e'		# kart -> telefon (bildirim)


def Crc16(Veri, Crc=0xFFFF):
    for b in Veri:
        Crc ^= b << 8
        for _ in range(8):
            Crc = ((Crc << 1) ^ 0x1021) & 0xFFFF if Crc & 0x8000 else (Crc << 1) & 0xFFFF
    return Crc


def Cerceve(Seq, Cmd, Yuk=b''):
    Govde = struct.pack('<HBB', len(Yuk), Seq, Cmd) + Yuk
    return bytes([SOF]) + Govde + struct.pack('<H', Crc16(Govde))


class Ayristirici:
    """Bayt akisindan gecerli cerceveleri cikarir; bozuk olani atlar."""
    def __init__(self):
        self.Tampon = bytearray()

    def Besle(self, Veri):
        self.Tampon += Veri
        Cikti = []
        while True:
            i = self.Tampon.find(SOF)
            if i < 0:
                self.Tampon.clear()
                break
            del self.Tampon[:i]
            if len(self.Tampon) < 3:
                break
            n = self.Tampon[1] | (self.Tampon[2] << 8)
            if n > 256:
                del self.Tampon[0]
                continue
            if len(self.Tampon) < n + 7:
                break
            Govde = bytes(self.Tampon[1:5 + n])
            Crc = self.Tampon[5 + n] | (self.Tampon[6 + n] << 8)
            if Crc16(Govde) != Crc:
                del self.Tampon[0]
                continue
            Cikti.append((Govde[2], Govde[3], Govde[4:]))
            del self.Tampon[:n + 7]
        return Cikti


# ============================================================================
#   Tasima katmani
# ============================================================================
class Tasima:
    Ad = ''

    def __init__(self):
        self.Alici = None			# fonksiyon(bytes)
        self.Kopma = None			# fonksiyon(mesaj)

    def Ac(self): pass
    def Kapat(self): pass
    def Gonder(self, Veri): pass


class TcpTasima(Tasima):
    Ad = 'WiFi'

    def __init__(self, Adres):
        super().__init__()
        Host, _, Port = Adres.partition(':')
        self.Host, self.Port = Host.strip(), int(Port or 5000)
        self.Soket = None
        self.Calis = False

    def Ac(self):
        self.Soket = socket.create_connection((self.Host, self.Port), timeout=3)
        self.Soket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.Soket.settimeout(0.2)
        self.Calis = True
        threading.Thread(target=self._Oku, daemon=True).start()

    def _Oku(self):
        while self.Calis:
            try:
                d = self.Soket.recv(4096)
            except socket.timeout:
                continue
            except OSError:
                d = None
            if not d:
                if self.Calis and self.Kopma:
                    self.Kopma('baglanti koptu')
                self.Calis = False
                return
            if self.Alici:
                self.Alici(d)

    def Gonder(self, Veri):
        self.Soket.sendall(Veri)

    def Kapat(self):
        self.Calis = False
        try:
            self.Soket.close()
        except Exception:
            pass


class SeriTasima(Tasima):
    Ad = 'UART'

    def __init__(self, Port, Baud=115200):
        super().__init__()
        self.PortAdi, self.Baud = Port, Baud
        self.Seri = None
        self.Calis = False

    @staticmethod
    def Portlar():
        try:
            from serial.tools import list_ports
            return [p.device for p in list_ports.comports()]
        except Exception:
            return []

    def Ac(self):
        import serial
        self.Seri = serial.Serial(self.PortAdi, self.Baud, timeout=0.1)
        self.Calis = True
        threading.Thread(target=self._Oku, daemon=True).start()

    def _Oku(self):
        while self.Calis:
            try:
                d = self.Seri.read(self.Seri.in_waiting or 1)
            except Exception:
                if self.Calis and self.Kopma:
                    self.Kopma('seri port kapandi')
                self.Calis = False
                return
            if d and self.Alici:
                self.Alici(d)

    def Gonder(self, Veri):
        self.Seri.write(Veri)

    def Kapat(self):
        self.Calis = False
        try:
            self.Seri.close()
        except Exception:
            pass


class BleTasima(Tasima):
    """BLE NUS -- bleak kutuphanesi, kendi asyncio dongusunde."""
    Ad = 'Bluetooth'

    def __init__(self, Adres):
        super().__init__()
        self.Adres = Adres
        self.Dongu = None
        self.Istemci = None
        self.Parca = 20

    @staticmethod
    def Tara(Sure=4.0):
        import asyncio
        from bleak import BleakScanner

        async def _Tara():
            Liste = await BleakScanner.discover(timeout=Sure, return_adv=True)
            return [(d.address, d.name or '?') for d, a in Liste.values()
                    if NUS_SERVIS in [u.lower() for u in (a.service_uuids or [])]]
        return asyncio.run(_Tara())

    def Ac(self):
        import asyncio
        from bleak import BleakClient

        Hazir = threading.Event()
        Sonuc = {}

        def Calistir():
            self.Dongu = asyncio.new_event_loop()
            asyncio.set_event_loop(self.Dongu)

            async def Baglan():
                self.Istemci = BleakClient(self.Adres, disconnected_callback=lambda c: self.Kopma and self.Kopma('BLE koptu'))
                await self.Istemci.connect(timeout=10)
                await self.Istemci.start_notify(NUS_OKU, lambda h, d: self.Alici and self.Alici(bytes(d)))
                try:
                    self.Parca = max(20, self.Istemci.mtu_size - 3)
                except Exception:
                    pass
            try:
                self.Dongu.run_until_complete(Baglan())
            except Exception as e:
                Sonuc['hata'] = e
            Hazir.set()
            if 'hata' not in Sonuc:
                self.Dongu.run_forever()
        threading.Thread(target=Calistir, daemon=True).start()
        Hazir.wait(15)
        if 'hata' in Sonuc:
            raise Sonuc['hata']

    def Gonder(self, Veri):
        import asyncio

        async def Yaz():
            for i in range(0, len(Veri), self.Parca):
                await self.Istemci.write_gatt_char(NUS_YAZ, Veri[i:i + self.Parca], response=False)
        asyncio.run_coroutine_threadsafe(Yaz(), self.Dongu).result(5)

    def Kapat(self):
        import asyncio
        if self.Dongu and self.Istemci:
            try:
                asyncio.run_coroutine_threadsafe(self.Istemci.disconnect(), self.Dongu).result(3)
            except Exception:
                pass
            self.Dongu.call_soon_threadsafe(self.Dongu.stop)


# ============================================================================
#   Cihaz -- komutlar
# ============================================================================
class HataYanit(Exception):
    def __init__(self, Kod):
        super().__init__(DURUM.get(Kod, str(Kod)))
        self.Kod = Kod


class Cihaz:
    """Komutlar es zamanli (cevabi bekler). Olay ve durum akisi geri cagriyla gelir
    (okuma thread'inden -- arayuz kendi kuyruguna aktarmali)."""
    ZAMAN_ASIMI = 0.3
    DENEME = 3

    def __init__(self, Tasima_):
        self.T = Tasima_
        self.P = Ayristirici()
        self.Seq = 0
        self.Kilit = threading.Lock()
        self.Bekleyen = {}			# seq -> (Event, [yanit])
        self.OlayGeri = None		# fonksiyon(Tur, Veri)
        self.DurumGeri = None		# fonksiyon(dict)
        self.T.Alici = self._Gelen

    def Ac(self):
        self.T.Ac()

    def Kapat(self):
        try:
            self.DurumAkisi(0)
        except Exception:
            pass
        self.T.Kapat()

    def _Gelen(self, Veri):
        for Seq, Cmd, Yuk in self.P.Besle(Veri):
            if Seq == 0 and Cmd == 0x60:
                if self.OlayGeri and Yuk:
                    self.OlayGeri(Yuk[0], Yuk[1:])
            elif Seq == 0 and Cmd == 0x70:
                if self.DurumGeri:
                    self.DurumGeri(self.DurumCoz(Yuk))
            else:
                b = self.Bekleyen.get(Seq)
                if b and Cmd & 0x80:
                    b[1].append((Cmd & 0x7F, Yuk))
                    b[0].set()

    def Komut(self, Cmd, Yuk=b'', Izin=(0,), Zaman=None, Deneme=None):
        with self.Kilit:
            self.Seq = self.Seq % 255 + 1
            Seq = self.Seq
            Olay = threading.Event()
            self.Bekleyen[Seq] = (Olay, [])
            try:
                for _ in range(Deneme or self.DENEME):
                    Olay.clear()
                    self.T.Gonder(Cerceve(Seq, Cmd, Yuk))
                    if Olay.wait(Zaman or self.ZAMAN_ASIMI):
                        for c, Cevap in self.Bekleyen[Seq][1]:
                            if c == Cmd and Cevap:
                                if Cevap[0] not in Izin:
                                    raise HataYanit(Cevap[0])
                                return Cevap[0], Cevap[1:]
                raise TimeoutError('kart cevap vermiyor (komut %02X)' % Cmd)
            finally:
                del self.Bekleyen[Seq]

    # --- Komutlar --------------------------------------------------------
    def Bilgi(self):
        _, d = self.Komut(0x01)
        Bicim = '<BHHHBBHBB'
        p, fw, w, h, bp, nm, npar, mod, guc = struct.unpack_from(Bicim, d)
        return dict(Protokol=p, Fw='%d.%d' % (fw >> 8, fw & 255), Genislik=w, Yukseklik=h,
                    Bitplane=bp, ModSayisi=nm, ParamSayisi=npar, Mod=mod, Guc=guc,
                    Ad=d[struct.calcsize(Bicim):].decode('utf-8', 'replace'))

    def Modlar(self):
        Liste, Bas = [], 0
        while True:
            _, d = self.Komut(0x10, bytes([Bas]))
            Toplam, b, Adet = d[0], d[1], d[2]
            i = 3
            for _ in range(Adet):
                No, Tur, Aile, n = d[i:i + 4]
                Liste.append(dict(No=No, Tur=Tur, Aile=Aile, Ad=d[i + 4:i + 4 + n].decode('utf-8', 'replace')))
                i += 4 + n
            Bas = b + Adet
            if Bas >= Toplam or Adet == 0:
                return Liste

    def Mod(self, Yeni=None):
        _, d = self.Komut(0x11, b'' if Yeni is None else bytes([Yeni]))
        return d[0]

    def Tanim(self, Sira):
        _, d = self.Komut(0x23, struct.pack('<H', Sira))
        Bicim = '<HHHBBIiiiiHhh'
        Sira, Toplam, Id, Tur, Bay, Maske, Mn, Mx, Adim, Vs, Gid, Galt, Gust = struct.unpack_from(Bicim, d)
        Metin = [m.decode('utf-8', 'replace') for m in d[struct.calcsize(Bicim):].split(b'\0')] + ['', '', '']
        return dict(Sira=Sira, Toplam=Toplam, Id=Id, Tur=TUR.get(Tur, '?'), Bayrak=Bay, Maske=Maske,
                    Min=Mn, Max=Mx, Adim=Adim, Varsayilan=Vs, GosterId=Gid, GosterAlt=Galt, GosterUst=Gust, Ad=Metin[0], Grup=Metin[1], Secenek=Metin[2])

    def Tanimlar(self):
        Ilk = self.Tanim(0)
        return [Ilk] + [self.Tanim(i) for i in range(1, Ilk['Toplam'])]

    def Al(self, Id, Mod=MOD_GENEL):
        _, d = self.Komut(0x20, struct.pack('<BH', Mod, Id))
        return struct.unpack_from('<i', d, 3)[0]

    def Yaz(self, Id, Deger, Mod=MOD_GENEL):
        St, d = self.Komut(0x21, struct.pack('<BHi', Mod, Id, int(Deger)), Izin=(0, 3))
        return St, struct.unpack_from('<i', d, 3)[0]

    def Hepsi(self, Mod=MOD_GENEL):
        _, d = self.Komut(0x22, bytes([Mod]))
        return {struct.unpack_from('<H', d, 2 + 6 * i)[0]: struct.unpack_from('<i', d, 4 + 6 * i)[0]
                for i in range(d[1])}

    def Kaydet(self):		self.Komut(0x30)
    def GeriYukle(self):	self.Komut(0x31)
    def Fabrika(self):		self.Komut(0x32)
    def AdVer(self, Ad):	self.Komut(0x02, Ad.encode('utf-8')[:16])
    def Ping(self):			self.Komut(0x03)

    def Guc(self, Acik=None):
        _, d = self.Komut(0x51, b'' if Acik is None else bytes([1 if Acik else 0]))
        return d[0]

    # --- ESP32 kopru: ev agi (CMD_BRIDGE 0x7F, STM32'ye gitmez) -----------
    def WifiTara(self):
        """Cevredeki aglar: [(Ad, Rssi, Sifreli)], guclu olan once."""
        _, d = self.Komut(0x7F, bytes([1]), Zaman=8.0, Deneme=1)
        Liste, i = [], 1
        for _ in range(d[0] if d else 0):
            Rssi, Sif, n = struct.unpack_from('<bBB', d, i)
            Liste.append((d[i + 3:i + 3 + n].decode('utf-8', 'replace'), Rssi, bool(Sif)))
            i += 3 + n
        return sorted(Liste, key=lambda a: -a[1])

    def WifiAyarla(self, Ad, Sifre):
        """Ev agi bilgilerini ESP32'ye kaydeder (Ad bos = unut)."""
        a, s = Ad.encode('utf-8')[:32], Sifre.encode('utf-8')[:64]
        self.Komut(0x7F, bytes([2, len(a)]) + a + bytes([len(s)]) + s, Zaman=1.0)

    def WifiDurum(self):
        _, d = self.Komut(0x7F, bytes([3]), Zaman=1.0)
        Durum, Rssi = d[0], struct.unpack_from('<b', d, 1)[0]
        Ip = '.'.join(str(b) for b in d[2:6])
        return dict(Durum=('kayit yok', 'baglaniyor', 'bagli')[Durum] if Durum < 3 else '?',
                    Rssi=Rssi, Ip=Ip, Ad=d[7:7 + d[6]].decode('utf-8', 'replace'))

    def KopruBilgi(self):
        """ESP32 kopru surumu, MAC, ad."""
        _, d = self.Komut(0x7F, bytes([0]), Zaman=1.0)
        Surum = struct.unpack_from('<H', d, 0)[0]
        return dict(Surum='%d.%d' % (Surum >> 8, Surum & 255), Mac=':'.join('%02X' % b for b in d[2:8]),
                    Ad=d[9:9 + d[8]].decode('utf-8', 'replace'))

    def KopruGuncelle(self, Veri, Ilerleme=None, Parca=240):
        """ESP32 yazilimini (.bin) yukler; bitince ESP32 yeniden baslar."""
        import hashlib
        self.Komut(0x7F, bytes([4]) + struct.pack('<I', len(Veri)) + hashlib.md5(Veri).digest(), Zaman=5.0, Deneme=1)
        Konum = 0
        try:
            while Konum < len(Veri):
                St, d = self.Komut(0x7F, bytes([5]) + struct.pack('<I', Konum) + Veri[Konum:Konum + Parca],
                                   Izin=(0, 3), Zaman=2.0)
                Yeni = struct.unpack_from('<I', d, 0)[0]
                if St == 3 and Yeni == Konum:
                    raise HataYanit(3)
                Konum = Yeni				# st 3: tekrar gonderilen parca zaten yazilmis
                if Ilerleme:
                    Ilerleme(Konum, len(Veri))
            self.Komut(0x7F, bytes([6]), Zaman=5.0, Deneme=1)
        except Exception:
            try:
                self.Komut(0x7F, bytes([7]), Zaman=1.0, Deneme=1)
            except Exception:
                pass
            raise

    def DurumAkisi(self, Ms):
        self.Komut(0x70, struct.pack('<H', Ms))

    @staticmethod
    def DurumCoz(Yuk):
        Alanlar = ['Mod', 'Tasma', 'IslemUs', 'CizimUs', 'AktarimUs', 'Kick', 'Vurus', 'Beat',
                   'Ses', 'Bas', 'Tiz', 'Bagil']
        try:
            return dict(zip(Alanlar, struct.unpack('<BIHHHBBBBBBB', Yuk[:21])))
        except struct.error:
            return {}
