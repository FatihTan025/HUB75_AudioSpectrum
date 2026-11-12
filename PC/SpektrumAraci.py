"""Spektrum Analizor -- PC ayar araci.

Kartla UART, WiFi (ESP32) veya Bluetooth (ESP32 BLE) uzerinden konusur.
Mod listesi ve ayar ekranlari karttan okunur; kartta yeni ayar eklenince
bu program degismeden gosterir.

Gerekenler: Python 3.9+, pip install pyserial bleak
"""
import queue, threading, time
import tkinter as tk
from tkinter import ttk, colorchooser, messagebox, filedialog

import protokol as P

# --- Renkler ----------------------------------------------------------------
ZEMIN = '#16181d'
PANEL = '#1f2229'
KENAR = '#2c3039'
YAZI = '#e6e8ec'
SOLUK = '#8b919c'
VURGU = '#ff7a1a'
YESIL = '#3ccf6e'
KIRMIZI = '#ff4d4d'
MAVI = '#4da3ff'

OLAY_KUTU = queue.Queue()			# thread'lerden arayuze


class Isci:
    """Kart komutlarini sirayla arka planda calistirir (arayuz donmasin)."""
    def __init__(self, Kok):
        self.Kok = Kok
        self.Is = queue.Queue()
        threading.Thread(target=self._Dongu, daemon=True).start()

    def Yap(self, Fonk, Sonra=None, Hata=None):
        self.Is.put((Fonk, Sonra, Hata))

    def _Dongu(self):
        while True:
            Fonk, Sonra, Hata = self.Is.get()
            try:
                Sonuc = Fonk()
                if Sonra:
                    OLAY_KUTU.put(lambda f=Sonra, s=Sonuc: f(s))
            except Exception as e:
                if Hata:
                    OLAY_KUTU.put(lambda f=Hata, e=e: f(e))


# ============================================================================
#   Ayar satirlari (karttan gelen tanima gore)
# ============================================================================
TON_GEN = 200


def TonRenk(Hu):
    import colorsys
    r, g, b = colorsys.hsv_to_rgb((Hu % 360) / 360.0, 1.0, 1.0)
    return '#%02x%02x%02x' % (int(r * 255), int(g * 255), int(b * 255))


class AyarSatiri:
    def __init__(self, Ust, Tanim, Uygulama, Mod):
        self.T, self.U, self.Mod = Tanim, Uygulama, Mod
        self.Cerceve = tk.Frame(Ust, bg=PANEL)
        self.Cerceve.pack(fill='x', padx=10, pady=3)
        tk.Label(self.Cerceve, text=Tanim['Ad'], bg=PANEL, fg=YAZI, width=22, anchor='w',
                 font=('Segoe UI', 10)).pack(side='left')
        self.Deger = None
        self.Gonderiliyor = False
        self.Bekleyen = None
        Tur = Tanim['Tur']
        Salt = Tanim['Bayrak'] & P.BAYRAK_SALT_OKUNUR

        if Tur == 'bool':
            self.Var = tk.IntVar()
            ttk.Checkbutton(self.Cerceve, variable=self.Var, style='Ayar.TCheckbutton',
                            command=lambda: self.Degisti(self.Var.get()),
                            state='disabled' if Salt else 'normal').pack(side='left')
        elif Tur == 'enum':
            self.Secenekler = Tanim['Secenek'].split('|')
            self.Var = tk.StringVar()
            k = ttk.Combobox(self.Cerceve, textvariable=self.Var, values=self.Secenekler,
                             state='disabled' if Salt else 'readonly', width=18)
            k.pack(side='left')
            k.bind('<<ComboboxSelected>>', lambda e: self.Degisti(self.Secenekler.index(self.Var.get())))
        elif Tur == 'blok':
            # Bloklu renk cubugu: birkac hazir renk, ara ton yok. Options "Ad:RRGGBB|..."
            self.Bloklar = [(p.split(':')[0], '#' + p.split(':')[1]) for p in Tanim['Secenek'].split('|') if ':' in p]
            n = max(1, len(self.Bloklar))
            self.Ton = tk.Canvas(self.Cerceve, width=TON_GEN, height=16, bg=PANEL, highlightthickness=0,
                                 cursor='hand2')
            for i, (_, r) in enumerate(self.Bloklar):
                self.Ton.create_rectangle(i * TON_GEN // n, 2, (i + 1) * TON_GEN // n - 1, 14, fill=r, outline='')
            self.TonImlec = self.Ton.create_rectangle(0, 0, 3, 16, outline='white', width=2)
            self.Ton.pack(side='left')
            self.Ton.bind('<Button-1>', self.BlokTik)
            self.Ton.bind('<B1-Motion>', self.BlokTik)
            self.BlokAd = tk.Label(self.Cerceve, bg=PANEL, fg=YAZI, width=10, anchor='w', font=('Segoe UI', 10))
            self.BlokAd.pack(side='left', padx=8)
        elif Tur in ('renk', 'ton'):
            # Pencere yok: gokkusagi ton cubugu (+ renkte hazir kutucuklar), anlik
            self.Ton = tk.Canvas(self.Cerceve, width=TON_GEN, height=16, bg=PANEL, highlightthickness=0,
                                 cursor='hand2')
            for x in range(TON_GEN):
                self.Ton.create_line(x, 2, x, 14, fill=TonRenk(x * 360 // TON_GEN))
            self.TonImlec = self.Ton.create_rectangle(0, 0, 3, 16, outline='white', width=2)
            self.Ton.pack(side='left')
            self.Ton.bind('<Button-1>', self.TonTik)
            self.Ton.bind('<B1-Motion>', self.TonTik)
            if Tur == 'renk':
                for Hazir in ('#ffffff', '#ff0000', '#ff8000', '#ffff00', '#00ff00', '#00ffff', '#0060ff', '#a000ff', '#ff00a0'):
                    k = tk.Label(self.Cerceve, bg=Hazir, width=2, cursor='hand2')
                    k.pack(side='left', padx=(6 if Hazir == '#ffffff' else 1, 0), ipady=1)
                    k.bind('<Button-1>', lambda e, h=Hazir: self.Degisti(int(h[1:], 16), Goster=True))
            self.Renk = tk.Label(self.Cerceve, width=3, bg=PANEL)
            self.Renk.pack(side='left', padx=8, ipady=1)
        else:
            self.Var = tk.DoubleVar()
            self.Olcek = tk.Scale(self.Cerceve, from_=Tanim['Min'], to=Tanim['Max'], variable=self.Var,
                                  orient='horizontal', length=220, showvalue=0, command=self.Kaydi,
                                  resolution=max(1, Tanim['Adim']), width=10, sliderlength=18,
                                  bg=VURGU, activebackground='#ffa25c', troughcolor=KENAR,
                                  sliderrelief='flat', bd=0, highlightthickness=0,
                                  state='disabled' if Salt else 'normal')
            self.Olcek.pack(side='left')
            self.Yazi = tk.Label(self.Cerceve, bg=PANEL, fg=VURGU, width=5, anchor='e',
                                 font=('Segoe UI Semibold', 10))
            self.Yazi.pack(side='left', padx=(8, 2))
            tk.Label(self.Cerceve, text=Tanim['Secenek'], bg=PANEL, fg=SOLUK,
                     font=('Segoe UI', 9)).pack(side='left')

        tk.Button(self.Cerceve, text='↺', bg=PANEL, fg=SOLUK, activebackground=KENAR,
                  activeforeground=YAZI, relief='flat', bd=0, highlightthickness=0, cursor='hand2', font=('Segoe UI', 11),
                  command=lambda: self.Degisti(Tanim['Varsayilan'], Goster=True)).pack(side='right')
        self.Cocuklar = [(w, {k: v for k, v in w.pack_info().items() if k != 'in'}) for w in self.Cerceve.pack_slaves()]
        self.Gorunur = True

    def GorunurYap(self, Acik):
        """Kosullu ayar: gizlenince satir 1 piksele iner (sira bozulmasin diye yerinde kalir)."""
        if Acik == self.Gorunur:
            return
        self.Gorunur = Acik
        for w, Bilgi in self.Cocuklar:
            if Acik:
                w.pack(**Bilgi)
            else:
                w.pack_forget()
        self.Cerceve.configure(height=1)
        self.Cerceve.pack_configure(pady=3 if Acik else 0)

    def Goster(self, v):
        self.Deger = v
        self.U.Gorunurluk()
        Tur = self.T['Tur']
        if Tur == 'bool':
            self.Var.set(1 if v else 0)
        elif Tur == 'enum':
            self.Var.set(self.Secenekler[v] if 0 <= v < len(self.Secenekler) else '?')
        elif Tur == 'blok':
            n = max(1, len(self.Bloklar))
            i = max(0, min(n - 1, v - self.T['Min']))
            self.Ton.coords(self.TonImlec, i * TON_GEN // n, 0, (i + 1) * TON_GEN // n - 1, 16)
            self.BlokAd.configure(text=self.Bloklar[i][0] if self.Bloklar else str(v))
        elif Tur in ('renk', 'ton'):
            r = TonRenk(v) if Tur == 'ton' else '#%06x' % (v & 0xFFFFFF)
            self.Renk.configure(bg=r)
            if Tur == 'ton':
                x = v * TON_GEN // 360
            else:
                import colorsys
                c = v & 0xFFFFFF
                x = int(colorsys.rgb_to_hsv((c >> 16) / 255, ((c >> 8) & 255) / 255, (c & 255) / 255)[0] * TON_GEN)
            self.Ton.coords(self.TonImlec, x - 1, 0, x + 2, 16)
        else:
            self.Var.set(v)
            self.Yazi.configure(text=str(v))

    def Kaydi(self, _):
        Adim = max(1, self.T['Adim'])
        v = self.T['Min'] + round((self.Var.get() - self.T['Min']) / Adim) * Adim
        v = max(self.T['Min'], min(self.T['Max'], int(v)))
        self.Yazi.configure(text=str(v))
        if v != self.Deger:
            self.Degisti(v)

    def BlokTik(self, e):
        n = max(1, len(self.Bloklar))
        v = self.T['Min'] + max(0, min(n - 1, e.x * n // TON_GEN))
        if v != self.Deger:
            self.Degisti(v, Goster=True)

    def TonTik(self, e):
        Hu = max(0, min(359, e.x * 360 // TON_GEN))
        v = Hu if self.T['Tur'] == 'ton' else int(TonRenk(Hu)[1:], 16)
        if v != self.Deger:
            self.Degisti(v, Goster=True)

    def Degisti(self, v, Goster=False):
        """Kaydirirken yuzlerce deger uretir: yalnizca sonuncusu gider."""
        if Goster:
            self.Goster(v)
        self.Deger = v
        self.U.Gorunurluk()
        self.Bekleyen = v
        if not self.Gonderiliyor:
            self._Gonder()

    def _Gonder(self):
        if self.Bekleyen is None or not self.U.Cihaz:
            self.Gonderiliyor = False
            return
        v, self.Bekleyen = self.Bekleyen, None
        self.Gonderiliyor = True
        Cihaz = self.U.Cihaz

        def Bitti(Sonuc):
            St, Uygulanan = Sonuc
            if self.Bekleyen is None:
                self.Goster(Uygulanan)
            self.U.Kirli(True)
            self._Gonder()
            if not self.Gonderiliyor:
                self.U.DegerYenile(self.Mod)	# bagli ayarlar (hazir set <-> cubuklar)

        def Hata(e):
            self.Gonderiliyor = False
            self.U.Durum('Ayar yazilamadi: %s' % e, KIRMIZI)

        self.U.Is.Yap(lambda: Cihaz.Yaz(self.T['Id'], v, self.Mod), Bitti, Hata)


# ============================================================================
#   Ana pencere
# ============================================================================
class Uygulama:
    def __init__(self):
        self.Kok = tk.Tk()
        self.Kok.title('Spektrum Aracı')
        self.Kok.geometry('1120x680')
        self.Kok.minsize(980, 600)
        self.Kok.configure(bg=ZEMIN)
        self.Is = Isci(self.Kok)
        self.Cihaz = None
        self.Tanimlar = []
        self.ModListe = []
        self.AktifMod = None
        self.Satirlar = {}			# (mod, id) -> AyarSatiri
        self.Lamba = {'Kick': 0.0, 'Vurus': 0.0, 'Beat': 0.0}
        self.Stil()
        self.Kur()
        self.Kok.after(30, self.KutuBosalt)
        self.Kok.after(40, self.LambaSon)
        self.Kok.protocol('WM_DELETE_WINDOW', self.Cikis)

    # --- Gorunum ----------------------------------------------------------
    def Stil(self):
        s = ttk.Style(self.Kok)
        s.theme_use('clam')
        s.configure('.', background=PANEL, foreground=YAZI, fieldbackground=KENAR, bordercolor=KENAR,
                    lightcolor=KENAR, darkcolor=KENAR, troughcolor=KENAR, arrowcolor=YAZI, font=('Segoe UI', 10))
        s.configure('TCombobox', fieldbackground=KENAR, background=KENAR, foreground=YAZI, padding=4)
        s.map('TCombobox', fieldbackground=[('readonly', KENAR)], foreground=[('readonly', YAZI)],
              selectbackground=[('readonly', KENAR)], selectforeground=[('readonly', YAZI)])
        self.Kok.option_add('*TCombobox*Listbox.background', KENAR)
        self.Kok.option_add('*TCombobox*Listbox.foreground', YAZI)
        self.Kok.option_add('*TCombobox*Listbox.selectBackground', VURGU)
        s.configure('Horizontal.TScale', background=VURGU, troughcolor=KENAR, sliderthickness=14)
        s.configure('Ayar.TCheckbutton', background=PANEL, indicatorbackground=KENAR, indicatorforeground=VURGU)
        s.map('Ayar.TCheckbutton', background=[('active', PANEL)], indicatorbackground=[('selected', VURGU)])
        s.configure('Treeview', background=PANEL, fieldbackground=PANEL, foreground=YAZI, rowheight=24,
                    borderwidth=0, font=('Segoe UI', 10))
        s.map('Treeview', background=[('selected', VURGU)], foreground=[('selected', '#111')])
        s.configure('Treeview.Heading', background=PANEL, foreground=SOLUK)
        s.layout('Treeview', [('Treeview.treearea', {'sticky': 'nswe'})])
        s.configure('Vertical.TScrollbar', background=KENAR, troughcolor=PANEL, arrowcolor=SOLUK)

    def Dugme(self, Ust, Yazi, Komut, Renk=KENAR, Yazi_Renk=YAZI, **k):
        b = tk.Button(Ust, text=Yazi, command=Komut, bg=Renk, fg=Yazi_Renk, activebackground=VURGU,
                      activeforeground='#111', relief='flat', bd=0, highlightthickness=0, padx=14, pady=6, cursor='hand2',
                      font=('Segoe UI Semibold', 10), disabledforeground='#5a606b', **k)
        return b

    def Baslik(self, Ust, Yazi):
        l = tk.Label(Ust, text=Yazi.upper(), bg=PANEL, fg=SOLUK, font=('Segoe UI Semibold', 9), anchor='w')
        l.pack(fill='x', padx=12, pady=(10, 4))
        return l

    def Kur(self):
        # --- Ust cubuk: baglanti --------------------------------------------
        Ust = tk.Frame(self.Kok, bg=ZEMIN)
        Ust.pack(fill='x', padx=12, pady=(12, 8))
        tk.Label(Ust, text='Spektrum', bg=ZEMIN, fg=VURGU, font=('Segoe UI Black', 15)).pack(side='left')
        tk.Label(Ust, text='Aracı', bg=ZEMIN, fg=YAZI, font=('Segoe UI Light', 15)).pack(side='left', padx=(4, 24))

        self.TurVar = tk.StringVar(value='UART')
        Tur = ttk.Combobox(Ust, textvariable=self.TurVar, values=['UART', 'WiFi', 'Bluetooth'],
                           state='readonly', width=10)
        Tur.pack(side='left')
        Tur.bind('<<ComboboxSelected>>', lambda e: self.TurDegisti())
        self.AdresVar = tk.StringVar()
        self.Adres = ttk.Combobox(Ust, textvariable=self.AdresVar, width=28)
        self.Adres.pack(side='left', padx=6)
        self.Tara = self.Dugme(Ust, 'Yenile', self.AdresYenile)
        self.Tara.pack(side='left')
        self.Baglan = self.Dugme(Ust, 'Bağlan', self.BaglanBirak, Renk=VURGU, Yazi_Renk='#111')
        self.Baglan.pack(side='left', padx=6)
        self.WifiD = self.Dugme(Ust, 'Wi-Fi ayarı', self.WifiPencere)
        self.WifiD.pack(side='left')
        self.CihazYazi = tk.Label(Ust, text='Bağlı değil', bg=ZEMIN, fg=SOLUK, font=('Segoe UI', 10))
        self.CihazYazi.pack(side='right')

        # --- Govde ------------------------------------------------------------
        Govde = tk.Frame(self.Kok, bg=ZEMIN)
        Govde.pack(fill='both', expand=True, padx=12)

        # Sol: modlar
        Sol = tk.Frame(Govde, bg=PANEL, width=230)
        Sol.pack(side='left', fill='y')
        Sol.pack_propagate(False)
        self.Baslik(Sol, 'Modlar')
        self.Agac = ttk.Treeview(Sol, show='tree', selectmode='browse')
        self.Agac.pack(fill='both', expand=True, padx=6, pady=(0, 8))
        self.Agac.bind('<<TreeviewSelect>>', self.ModSecildi)

        # Sag: canli durum
        Sag = tk.Frame(Govde, bg=PANEL, width=250)
        Sag.pack(side='right', fill='y')
        Sag.pack_propagate(False)
        self.CanliKur(Sag)

        # Orta: ayarlar (kaydirilabilir)
        Orta = tk.Frame(Govde, bg=PANEL)
        Orta.pack(side='left', fill='both', expand=True, padx=10)
        self.Tuval = tk.Canvas(Orta, bg=PANEL, highlightthickness=0)
        Kaydir = ttk.Scrollbar(Orta, orient='vertical', command=self.Tuval.yview)
        self.AyarAlan = tk.Frame(self.Tuval, bg=PANEL)
        self.AyarAlan.bind('<Configure>', lambda e: self.Tuval.configure(scrollregion=self.Tuval.bbox('all')))
        self.TuvalPencere = self.Tuval.create_window((0, 0), window=self.AyarAlan, anchor='nw')
        self.Tuval.bind('<Configure>', lambda e: self.Tuval.itemconfigure(self.TuvalPencere, width=e.width))
        self.Tuval.configure(yscrollcommand=Kaydir.set)
        self.Tuval.pack(side='left', fill='both', expand=True)
        Kaydir.pack(side='right', fill='y')
        self.Tuval.bind_all('<MouseWheel>', lambda e: self.Tuval.yview_scroll(int(-e.delta / 120), 'units'))
        self.Tuval.bind_all('<Button-4>', lambda e: self.Tuval.yview_scroll(-2, 'units'))
        self.Tuval.bind_all('<Button-5>', lambda e: self.Tuval.yview_scroll(2, 'units'))
        self.BosEkran()

        # --- Alt cubuk ----------------------------------------------------------
        Alt = tk.Frame(self.Kok, bg=ZEMIN)
        Alt.pack(fill='x', padx=12, pady=10)
        self.Kaydet = self.Dugme(Alt, 'Kaydet', self.KaydetBas, Renk=VURGU, Yazi_Renk='#111')
        self.Kaydet.pack(side='left')
        self.GeriAl = self.Dugme(Alt, 'Geri al', self.GeriAlBas)
        self.GeriAl.pack(side='left', padx=6)
        self.FabrikaD = self.Dugme(Alt, 'Fabrika ayarı', self.FabrikaBas)
        self.FabrikaD.pack(side='left')
        self.GucD = self.Dugme(Alt, 'Ekranı kapat', self.GucBas)
        self.GucD.pack(side='left', padx=6)
        tk.Label(Alt, text='Cihaz adı', bg=ZEMIN, fg=SOLUK).pack(side='left', padx=(18, 6))
        self.AdVar = tk.StringVar()
        tk.Entry(Alt, textvariable=self.AdVar, width=17, bg=KENAR, fg=YAZI, insertbackground=YAZI,
                 relief='flat', font=('Segoe UI', 10)).pack(side='left', ipady=5)
        self.AdD = self.Dugme(Alt, 'Değiştir', self.AdBas)
        self.AdD.pack(side='left', padx=6)
        self.DurumYazi = tk.Label(Alt, text='', bg=ZEMIN, fg=SOLUK, anchor='e')
        self.DurumYazi.pack(side='right', fill='x', expand=True)
        self.KirliYazi = tk.Label(Alt, text='', bg=ZEMIN, fg=VURGU, font=('Segoe UI Semibold', 9))
        self.KirliYazi.pack(side='right', padx=10)

        self.Etkin(False)
        self.TurDegisti()

    def CanliKur(self, Ust):
        self.Baslik(Ust, 'Canlı durum')
        self.CanliVar = tk.IntVar(value=1)
        ttk.Checkbutton(Ust, text=' Akışı göster', variable=self.CanliVar, style='Ayar.TCheckbutton',
                        command=self.CanliDegisti).pack(anchor='w', padx=12)
        self.CanliT = tk.Canvas(Ust, bg=PANEL, highlightthickness=0, height=190)
        self.CanliT.pack(fill='x', padx=12, pady=8)
        self.Cubuk = {}
        for i, (Ad, Renk) in enumerate([('Ses', YAZI), ('Bas', VURGU), ('Tiz', MAVI), ('Bağıl', YESIL)]):
            x = 10 + i * 56
            self.CanliT.create_rectangle(x, 10, x + 34, 150, fill=KENAR, outline='')
            self.Cubuk[Ad] = (self.CanliT.create_rectangle(x, 150, x + 34, 150, fill=Renk, outline=''), x)
            self.CanliT.create_text(x + 17, 166, text=Ad, fill=SOLUK, font=('Segoe UI', 9))
        self.Lambalar = {}
        Lf = tk.Frame(Ust, bg=PANEL)
        Lf.pack(fill='x', padx=12)
        for Ad, Renk in [('Kick', VURGU), ('Vurus', MAVI), ('Beat', YESIL)]:
            c = tk.Canvas(Lf, width=70, height=58, bg=PANEL, highlightthickness=0)
            c.pack(side='left')
            o = c.create_oval(21, 4, 49, 32, fill=KENAR, outline='')
            c.create_text(35, 46, text={'Vurus': 'Vuruş'}.get(Ad, Ad), fill=SOLUK, font=('Segoe UI', 9))
            self.Lambalar[Ad] = (c, o, Renk)
        self.Baslik(Ust, 'Süreler')
        self.SureYazi = tk.Label(Ust, text='-', bg=PANEL, fg=YAZI, justify='left', anchor='w',
                                 font=('Consolas', 10))
        self.SureYazi.pack(fill='x', padx=12)

    def BosEkran(self, Yazi='Karta bağlanınca ayarlar burada görünür.'):
        for w in self.AyarAlan.winfo_children():
            w.destroy()
        tk.Label(self.AyarAlan, text=Yazi, bg=PANEL, fg=SOLUK, font=('Segoe UI', 11)).pack(pady=60)

    def Etkin(self, Acik):
        for d in (self.Kaydet, self.GeriAl, self.FabrikaD, self.GucD, self.AdD, self.WifiD):
            d.configure(state='normal' if Acik else 'disabled')

    def Durum(self, Yazi, Renk=SOLUK):
        self.DurumYazi.configure(text=Yazi, fg=Renk)

    def Kirli(self, Var):
        self.KirliYazi.configure(text='● Kaydedilmedi' if Var else '')

    # --- Baglanti ----------------------------------------------------------
    def TurDegisti(self):
        Tur = self.TurVar.get()
        self.Tara.configure(text='Tara' if Tur == 'Bluetooth' else 'Yenile', state='normal' if Tur != 'WiFi' else 'disabled')
        if Tur == 'WiFi':
            self.Adres.configure(values=['192.168.4.1:5000', 'spektrum.local:5000', '127.0.0.1:5000'])
            self.AdresVar.set('192.168.4.1:5000')	# ESP32 kendi agi (Spektrum-XXXX)
        else:
            self.Adres.configure(values=[])
            self.AdresVar.set('')
            self.AdresYenile()

    def AdresYenile(self):
        Tur = self.TurVar.get()
        if Tur == 'UART':
            Liste = P.SeriTasima.Portlar()
            self.Adres.configure(values=Liste)
            if Liste and not self.AdresVar.get():
                self.AdresVar.set(Liste[0])
            self.Durum('%d seri port bulundu' % len(Liste))
        elif Tur == 'Bluetooth':
            self.Durum('Bluetooth cihazları aranıyor...')
            self.Tara.configure(state='disabled')

            def Bitti(Liste):
                self.Tara.configure(state='normal')
                Degerler = ['%s  (%s)' % (a, n) for a, n in Liste]
                self.Adres.configure(values=Degerler)
                if Degerler:
                    self.AdresVar.set(Degerler[0])
                self.Durum('%d cihaz bulundu' % len(Liste))

            def Hata(e):
                self.Tara.configure(state='normal')
                self.Durum('Bluetooth taraması olmadı: %s' % e, KIRMIZI)
            self.Is.Yap(P.BleTasima.Tara, Bitti, Hata)

    def BaglanBirak(self):
        if self.Cihaz:
            self.Birak()
            return
        Tur, Adres = self.TurVar.get(), self.AdresVar.get().strip()
        if not Adres:
            self.Durum('Adres / port seç', KIRMIZI)
            return
        if Tur == 'UART':
            T = P.SeriTasima(Adres)
        elif Tur == 'WiFi':
            T = P.TcpTasima(Adres)
        else:
            T = P.BleTasima(Adres.split()[0])
        C = P.Cihaz(T)
        T.Kopma = lambda m: OLAY_KUTU.put(lambda: self.Koptu(m))
        C.OlayGeri = lambda Tur_, Veri: OLAY_KUTU.put(lambda: self.Olay(Tur_, Veri))
        C.DurumGeri = lambda d: OLAY_KUTU.put(lambda: self.CanliGoster(d))
        self.Baglan.configure(state='disabled')
        self.Durum('Bağlanılıyor...')

        def Yukle():
            C.Ac()
            Bilgi = C.Bilgi()
            return Bilgi, C.Modlar(), C.Tanimlar()

        def Bitti(Sonuc):
            self.Cihaz = C
            self.Baglan.configure(state='normal', text='Bağlantıyı kes', bg=KENAR, fg=YAZI)
            self.Kur_Cihaz(*Sonuc)

        def Hata(e):
            try:
                T.Kapat()
            except Exception:
                pass
            self.Baglan.configure(state='normal')
            self.Durum('Bağlanılamadı: %s' % e, KIRMIZI)
        self.Is.Yap(Yukle, Bitti, Hata)

    def Birak(self, Mesaj='Bağlantı kesildi'):
        C, self.Cihaz = self.Cihaz, None
        if C:
            self.Is.Yap(C.Kapat)
        self.Baglan.configure(text='Bağlan', bg=VURGU, fg='#111', state='normal')
        self.CihazYazi.configure(text='Bağlı değil', fg=SOLUK)
        self.Agac.delete(*self.Agac.get_children())
        self.Satirlar = {}
        self.BosEkran()
        self.Etkin(False)
        self.Kirli(False)
        self.Durum(Mesaj)

    def Koptu(self, Mesaj):
        if self.Cihaz:
            self.Birak('Bağlantı koptu: %s' % Mesaj)

    def Kur_Cihaz(self, Bilgi, Modlar, Tanimlar):
        self.Bilgi, self.ModListe, self.Tanimlar = Bilgi, Modlar, Tanimlar
        self.CihazYazi.configure(text='%s   ·   %d×%d   ·   fw %s' % (
            Bilgi['Ad'], Bilgi['Genislik'], Bilgi['Yukseklik'], Bilgi['Fw']), fg=YAZI)
        self.AdVar.set(Bilgi['Ad'])
        self.GucGoster(Bilgi['Guc'])
        self.Agac.delete(*self.Agac.get_children())
        Ustler = {}
        for m in Modlar:
            Grup = 'Müziksiz' if m['Tur'] == 1 else P.AILE.get(m['Aile'], 'Diğer')
            if Grup not in Ustler:
                Ustler[Grup] = self.Agac.insert('', 'end', text=Grup.upper(), open=True, tags=('grup',))
            self.Agac.insert(Ustler[Grup], 'end', iid='m%d' % m['No'], text='%2d   %s' % (m['No'], m['Ad']))
        self.Agac.tag_configure('grup', foreground=SOLUK, font=('Segoe UI Semibold', 9))
        self.Etkin(True)
        self.Kirli(False)
        self.ModGoster(Bilgi['Mod'])
        self.CanliDegisti()
        self.Durum('Bağlandı: %d mod, %d ayar' % (len(Modlar), len(Tanimlar)), YESIL)

    # --- Modlar ve ayarlar ---------------------------------------------------
    def ModGoster(self, Mod):
        """Secimi gunceller ve ayar ekranini o moda gore kurar."""
        self.AktifMod = Mod
        iid = 'm%d' % Mod
        if self.Agac.exists(iid) and self.Agac.selection() != (iid,):
            self._SecimSessiz = True
            self.Agac.selection_set(iid)
            self.Agac.see(iid)
        self.AyarlariKur()

    def ModSecildi(self, _):
        if getattr(self, '_SecimSessiz', False):
            self._SecimSessiz = False
            return
        Sec = self.Agac.selection()
        if not Sec or not Sec[0].startswith('m') or not self.Cihaz:
            return
        Mod = int(Sec[0][1:])
        if Mod == self.AktifMod:
            return
        C = self.Cihaz
        self.Is.Yap(lambda: C.Mod(Mod), lambda m: (self.Kirli(True), self.ModGoster(m)),
                    lambda e: self.Durum('Mod seçilemedi: %s' % e, KIRMIZI))

    def AyarlariKur(self):
        C, Mod = self.Cihaz, self.AktifMod
        Genel = [t for t in self.Tanimlar if not t['Bayrak'] & P.BAYRAK_MOD]
        ModAyar = [t for t in self.Tanimlar if t['Bayrak'] & P.BAYRAK_MOD]

        def Oku():
            return C.Hepsi(P.MOD_GENEL), C.Hepsi(Mod)

        def Bitti(Sonuc):
            if Mod != self.AktifMod:
                return
            Gd, Md = Sonuc
            Liste = [t for t in ModAyar if t['Id'] in Md]	# kart bu gorunumde olanlari gonderir
            for w in self.AyarAlan.winfo_children():
                w.destroy()
            self.Satirlar = {}
            Ad = next((m['Ad'] for m in self.ModListe if m['No'] == Mod), str(Mod))
            self.Bolum('Mod ayarları  ·  %s' % Ad, Liste, Md, Mod,
                       'Bu modun kendine ait ayarı yok.')
            self.Bolum('Genel ayarlar', Genel, Gd, P.MOD_GENEL, '')
            self.Tuval.yview_moveto(0)
        self.Is.Yap(Oku, Bitti, lambda e: self.Durum('Ayarlar okunamadı: %s' % e, KIRMIZI))

    def Bolum(self, Baslik, Liste, Degerler, Mod, Bos):
        tk.Label(self.AyarAlan, text=Baslik, bg=PANEL, fg=YAZI, anchor='w',
                 font=('Segoe UI Semibold', 12)).pack(fill='x', padx=12, pady=(14, 2))
        if not Liste:
            tk.Label(self.AyarAlan, text=Bos, bg=PANEL, fg=SOLUK, anchor='w').pack(fill='x', padx=12, pady=4)
            return
        Gruplar = {}
        for t in Liste:
            Gruplar.setdefault(t['Grup'] or 'Diğer', []).append(t)
        for Grup, Ogeler in Gruplar.items():
            if not all(t['Bayrak'] & P.BAYRAK_GIZLI for t in Ogeler):
                tk.Label(self.AyarAlan, text=Grup.upper(), bg=PANEL, fg=SOLUK, anchor='w',
                         font=('Segoe UI Semibold', 8)).pack(fill='x', padx=12, pady=(10, 2))
                tk.Frame(self.AyarAlan, bg=KENAR, height=1).pack(fill='x', padx=12, pady=(0, 4))
            for t in Ogeler:
                s = AyarSatiri(self.AyarAlan, t, self, Mod)
                s.Gizli = bool(t['Bayrak'] & P.BAYRAK_GIZLI)	# gorunume sabit: gosterilmez, kosul icin tutulur
                if t['Id'] in Degerler:
                    s.Goster(Degerler[t['Id']])
                self.Satirlar[(Mod, t['Id'])] = s
        self.Gorunurluk()

    def Gorunurluk(self):
        """Karttan gelen kosul: GosterId ayari GosterAlt..GosterUst arasindaysa goster.
        Bagli oldugu ayar gizliyse bu da gizlenir (zincir)."""
        def Acik(s, Iz=0):
            g = s.T.get('GosterId', 0)
            if not g or Iz > 4:
                return True
            Bag = self.Satirlar.get((s.Mod, g))
            if Bag is None or Bag.Deger is None:
                return True
            return s.T['GosterAlt'] <= Bag.Deger <= s.T['GosterUst'] and Acik(Bag, Iz + 1)
        for s in list(self.Satirlar.values()):
            s.GorunurYap(not getattr(s, 'Gizli', False) and Acik(s))

    def DegerYenile(self, Mod):
        """Yazma bitince kart bagli ayarlari degistirmis olabilir: kisa bekleyip hepsini oku."""
        if getattr(self, '_YenileIs', None):
            self.Kok.after_cancel(self._YenileIs)
        self._YenileIs = self.Kok.after(250, lambda: self._Yenile(Mod))

    def _Yenile(self, Mod):
        self._YenileIs = None
        C = self.Cihaz
        if not C:
            return

        def Bitti(D):
            for (m, Id), s in list(self.Satirlar.items()):
                if m == Mod and Id in D and not s.Gonderiliyor and s.Bekleyen is None and D[Id] != s.Deger:
                    s.Goster(D[Id])
        self.Is.Yap(lambda: C.Hepsi(Mod), Bitti)

    # --- Ev agi (ESP32 kopru) -----------------------------------------------
    def WifiPencere(self):
        """Kart Wi-Fi ile (ESP32 kendi agi) bagliyken: ev agini sec, sifreyi gir."""
        C = self.Cihaz
        P = tk.Toplevel(self.Kok, bg=PANEL)
        P.title('Ev ağı')
        P.geometry('380x440')
        P.transient(self.Kok)
        self.Baslik(P, 'Çevredeki ağlar')
        Liste = tk.Listbox(P, bg=KENAR, fg=YAZI, selectbackground=VURGU, relief='flat',
                           highlightthickness=0, font=('Segoe UI', 10), height=8)
        Liste.pack(fill='x', padx=12)
        Satir = tk.Frame(P, bg=PANEL)
        Satir.pack(fill='x', padx=12, pady=(10, 4))
        tk.Label(Satir, text='Ağ adı', bg=PANEL, fg=SOLUK, width=8, anchor='w').pack(side='left')
        AdV = tk.StringVar()
        tk.Entry(Satir, textvariable=AdV, bg=KENAR, fg=YAZI, insertbackground=YAZI, relief='flat').pack(side='left', fill='x', expand=True, ipady=4)
        Satir2 = tk.Frame(P, bg=PANEL)
        Satir2.pack(fill='x', padx=12, pady=4)
        tk.Label(Satir2, text='Şifre', bg=PANEL, fg=SOLUK, width=8, anchor='w').pack(side='left')
        SifV = tk.StringVar()
        tk.Entry(Satir2, textvariable=SifV, show='•', bg=KENAR, fg=YAZI, insertbackground=YAZI, relief='flat').pack(side='left', fill='x', expand=True, ipady=4)
        Durum = tk.Label(P, text='', bg=PANEL, fg=SOLUK, anchor='w', justify='left')
        Alt = tk.Frame(P, bg=PANEL)
        Alt.pack(fill='x', padx=12, pady=8)
        Aglar = []

        def Sec(_):
            s = Liste.curselection()
            if s:
                AdV.set(Aglar[s[0]][0])

        def Tara():
            Durum.configure(text='Taranıyor...', fg=SOLUK)

            def Bitti(L):
                Aglar[:] = L
                Liste.delete(0, 'end')
                for Ad, Rssi, Sif in L:
                    Liste.insert('end', '%s   %d dBm%s' % (Ad, Rssi, '  🔒' if Sif else ''))
                Durum.configure(text='%d ağ bulundu' % len(L))
            self.Is.Yap(C.WifiTara, Bitti, lambda e: Durum.configure(text='Taranamadı: %s' % e, fg=KIRMIZI))

        def Goster(d):
            Durum.configure(text='Ev ağı: %s  ·  %s%s' % (d['Ad'] or '-', d['Durum'],
                            ('  ·  %s  (%d dBm)\nPC ev ağındayken adres: %s:5000' % (d['Ip'], d['Rssi'], d['Ip'])) if d['Durum'] == 'bagli' else ''),
                            fg=YESIL if d['Durum'] == 'bagli' else SOLUK)

        def DurumSor():
            self.Is.Yap(C.WifiDurum, Goster, lambda e: Durum.configure(text=str(e), fg=KIRMIZI))

        def Kaydet():
            Ad, Sif = AdV.get().strip(), SifV.get()
            if not Ad:
                return
            Durum.configure(text='Kaydediliyor, bağlanılıyor...', fg=SOLUK)
            self.Is.Yap(lambda: C.WifiAyarla(Ad, Sif), lambda _: P.after(6000, DurumSor),
                        lambda e: Durum.configure(text='Kaydedilemedi: %s' % e, fg=KIRMIZI))

        def Guncelle():
            Dosya = filedialog.askopenfilename(parent=P, title='ESP32 yazılımı', filetypes=[('ESP32 .bin', '*.bin')])
            if not Dosya:
                return
            with open(Dosya, 'rb') as d:
                Veri = d.read()
            Oran = [0, len(Veri), True]

            def Izle():
                if Oran[2]:
                    Durum.configure(text='ESP32 güncelleniyor... %%%d' % (100 * Oran[0] // max(1, Oran[1])), fg=SOLUK)
                    P.after(300, Izle)

            def Bitti(_):
                Oran[2] = False
                Durum.configure(text='ESP32 güncellendi, yeniden başlıyor. Yeniden bağlanın.', fg=YESIL)

            def Hata(e):
                Oran[2] = False
                Durum.configure(text='Güncellenemedi: %s' % e, fg=KIRMIZI)
            Izle()
            self.Is.Yap(lambda: C.KopruGuncelle(Veri, lambda a, b: Oran.__setitem__(0, a)), Bitti, Hata)

        Liste.bind('<<ListboxSelect>>', Sec)
        self.Dugme(Alt, 'Tara', Tara).pack(side='left')
        self.Dugme(Alt, 'Bağlan', Kaydet, Renk=VURGU, Yazi_Renk='#111').pack(side='left', padx=6)
        self.Dugme(Alt, 'Durum', DurumSor).pack(side='left')
        self.Dugme(Alt, 'ESP32 güncelle', Guncelle).pack(side='right')
        Surum = tk.Label(P, text='', bg=PANEL, fg=SOLUK, anchor='w')
        Durum.pack(fill='x', padx=12)
        Surum.pack(fill='x', padx=12, pady=(6, 0))
        DurumSor()
        self.Is.Yap(C.KopruBilgi, lambda b: Surum.configure(text='Köprü %s  ·  %s  ·  %s' % (b['Ad'], b['Surum'], b['Mac'])),
                    lambda e: Surum.configure(text='Köprü sürümü okunamadı (eski köprü)'))

    # --- Alt dugmeler --------------------------------------------------------
    def KaydetBas(self):
        C = self.Cihaz
        self.Is.Yap(C.Kaydet, lambda _: (self.Kirli(False), self.Durum('Ayarlar karta kaydedildi', YESIL)),
                    lambda e: self.Durum('Kaydedilemedi: %s' % e, KIRMIZI))

    def GeriAlBas(self):
        C = self.Cihaz
        self.Is.Yap(lambda: (C.GeriYukle(), C.Mod())[1],
                    lambda m: (self.Kirli(False), self.ModGoster(m), self.Durum('Kayıtlı ayarlara dönüldü')),
                    lambda e: self.Durum('Geri alınamadı: %s' % e, KIRMIZI))

    def FabrikaBas(self):
        if not messagebox.askyesno('Fabrika ayarı',
                                   'Tüm ayarlar fabrika değerlerine dönsün mü?\n'
                                   '(Kaydet\'e basmadan kalıcı olmaz.)', parent=self.Kok):
            return
        C = self.Cihaz
        self.Is.Yap(lambda: (C.Fabrika(), C.Mod())[1],
                    lambda m: (self.Kirli(True), self.ModGoster(m), self.Durum('Fabrika ayarları yüklendi')),
                    lambda e: self.Durum('Olmadı: %s' % e, KIRMIZI))

    def GucGoster(self, Acik):
        self.GucAcik = bool(Acik)
        self.GucD.configure(text='Ekranı kapat' if Acik else 'Ekranı aç')

    def GucBas(self):
        C, Yeni = self.Cihaz, not self.GucAcik
        self.Is.Yap(lambda: C.Guc(Yeni), self.GucGoster, lambda e: self.Durum(str(e), KIRMIZI))

    def AdBas(self):
        C, Ad = self.Cihaz, self.AdVar.get().strip()[:16]
        if not Ad:
            return

        def Bitti(Bilgi):
            self.Bilgi = Bilgi
            self.CihazYazi.configure(text='%s   ·   %d×%d   ·   fw %s' % (
                Bilgi['Ad'], Bilgi['Genislik'], Bilgi['Yukseklik'], Bilgi['Fw']))
            self.Kirli(True)
        self.Is.Yap(lambda: (C.AdVer(Ad), C.Bilgi())[1], Bitti, lambda e: self.Durum(str(e), KIRMIZI))

    # --- Karttan gelenler ------------------------------------------------------
    def Olay(self, Tur, Veri):
        if Tur == P.EVT_MOD and Veri:
            self.ModGoster(Veri[0])
            self.Durum('Kartta mod değişti: %d' % Veri[0])
        elif Tur == P.EVT_ACILIS and self.Cihaz:
            self.Durum('Kart yeniden başladı, ayarlar yenileniyor', VURGU)
            C = self.Cihaz
            self.Is.Yap(lambda: (C.Bilgi(), C.Modlar(), C.Tanimlar()), lambda s: self.Kur_Cihaz(*s))
        elif Tur == P.EVT_GUC and Veri:
            self.GucGoster(Veri[0])
        elif Tur == P.EVT_PARAM and len(Veri) >= 7:		# Kartta (encoder/menu) ayar degisti
            import struct
            Mod, Id, Deger = struct.unpack_from('<BHi', Veri)
            Satir = self.Satirlar.get((Mod, Id))
            if Satir and not Satir.Gonderiliyor:
                Satir.Goster(Deger)

    def CanliDegisti(self):
        if not self.Cihaz:
            return
        C, Ms = self.Cihaz, 100 if self.CanliVar.get() else 0
        self.Is.Yap(lambda: C.DurumAkisi(Ms))
        if not Ms:
            self.CanliGoster({'Ses': 0, 'Bas': 0, 'Tiz': 0, 'Bagil': 0})
            self.SureYazi.configure(text='-')

    def CanliGoster(self, d):
        if not d:
            return
        for Ad, Anahtar in [('Ses', 'Ses'), ('Bas', 'Bas'), ('Tiz', 'Tiz'), ('Bağıl', 'Bagil')]:
            Id, x = self.Cubuk[Ad]
            y = 150 - int(d.get(Anahtar, 0) * 140 / 255)
            self.CanliT.coords(Id, x, y, x + 34, 150)
        for Ad in self.Lamba:
            if d.get(Ad):
                self.Lamba[Ad] = 1.0
        if 'IslemUs' in d:
            self.SureYazi.configure(text='İşlem   %5d µs\nÇizim    %5d µs\nAktarım  %5d µs\nTaşma    %5d' % (
                d['IslemUs'], d['CizimUs'], d['AktarimUs'], d['Tasma']))

    def LambaSon(self):
        for Ad, (c, o, Renk) in self.Lambalar.items():
            v = self.Lamba[Ad]
            c.itemconfigure(o, fill=Renk if v > 0.5 else (KENAR if v <= 0 else '#5a3a20' if Ad == 'Kick' else '#2a3f55' if Ad == 'Vurus' else '#245536'))
            self.Lamba[Ad] = max(0.0, v - 0.25)
        self.Kok.after(40, self.LambaSon)

    def KutuBosalt(self):
        try:
            while True:
                OLAY_KUTU.get_nowait()()
        except queue.Empty:
            pass
        self.Kok.after(30, self.KutuBosalt)

    def Cikis(self):
        if self.Cihaz:
            try:
                self.Cihaz.Kapat()
            except Exception:
                pass
        self.Kok.destroy()

    def Calistir(self):
        self.Kok.mainloop()


if __name__ == '__main__':
    Uygulama().Calistir()
