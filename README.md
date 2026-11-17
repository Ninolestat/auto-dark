# AutoDark

Applicazione nativa Win32 ultra-leggera per Windows 10/11 che cambia automaticamente tra tema **Chiaro** e **Scuro** (e opzionalmente lo sfondo del desktop) all'alba/tramonto o a un orario fisso.

- **Consumo RAM minimo in systray:** ~`0.2 MB` Working Set / ~`2.6 MB` Private Memory
- **Eseguibile portatile:** ~`85 KB` (zero dipendenze esterne, scritta in C puro con Win32 API)
- **Installer incluso:** ~`156 KB` (`AutoDarkSetup.exe`) con supporto a *Avvio automatico con Windows* in systray e disinstallazione pulita da *App installate*

## Funzionalità

- **Alba / Tramonto automatico:** calcolo astronomico locale (algoritmo NOAA) senza chiamate di rete né permessi di geolocalizzazione, con 20 città preimpostate.
- **Orario fisso:** programmazione rapida per tema Chiaro e Scuro.
- **Cambio sfondo opzionale:** preimpostato su `C:\Windows\Web\Wallpaper\Windows\img0.jpg` (Chiaro) e `img19.jpg` (Scuro), personalizzabile con qualsiasi immagine.
- **Tasti rapidi:** switch immediato `Chiaro` / `Scuro` dal popup o dal menu della systray.
- **Grafica minimale adattiva:** popup con angoli arrotondati di Windows 11 che si adatta automaticamente al tema Chiaro o Scuro attivo.

## Installazione

Scarica `AutoDarkSetup.exe` dalla sezione [Releases](../../releases) ed eseguilo.

## Compilazione da sorgente

Requisiti: [Zig](https://ziglang.org/) (`zig cc`) su Windows.

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

Genera `build/AutoDark.exe` e l'installer autonomo `build/AutoDarkSetup.exe`.
