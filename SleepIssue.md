# Phân tích dòng shutdown BQ76952

> hãy đọc datasheet và RM của bq76952, sau đó xem file mạch nguyên lý, và phân tích tìm hiểu nguyên nhân xem vì sao khi bq ở chế độ shutdown dòng tiêu thụ lại là 70uA, khi ở chế độ deepsleep dòng tiêu thụ 115uA, (các led đang k hàn, khối LM5163 k hàn, chỉ dùng nguồn 5V của BQ, khi shutdown thì nguồn 5V cx off), khi tháo BQ ra thì dòng về 0

<details><summary>37 previous messages</summary>

> Mình sẽ đối chiếu ba phần song song: thông số dòng ở SHUTDOWN/DEEPSLEEP trong datasheet–TRM, các điều kiện thật sự để BQ76952 vào đúng mode, và mọi đường rò/cấp ngược trên schematic. Sau đó mình sẽ quy đổi các mức 70 µA và 115 µA thành những nhánh dòng khả dĩ để khoanh vùng bằng phép đo.
>
> <details><summary>Ran 5 commands</summary>
>
> - Ran `rg --files`
> - Ran `rg --files -g "*.pdf" -g "*.PDF" -g "*.xlsx" -g "*.xls" -g "*.csv" -g "*.PrjPcb" -g "*.SchDoc" -g "*.kicad_sch"`
> - Ran `Get-ChildItem -Force | Select-Object Mode,Length,Name`
> - Ran `Get-ChildItem -LiteralPath "Data_BMS_10S_V1.0_280426" -Recurse -File | Select-Object FullName,Length`
> - Ran `Get-ChildItem -Recurse -File -Include *bq76952*,*BQ76952*,*.pdf,*.PDF | Select-Object FullName,Length`
>
> </details>
>
> <details><summary>Ran 11 commands, searched the web</summary>
>
> <details><summary>Searched the web</summary>
>
> - Searched the web for `site:ti.com/lit/ds bq76952 datasheet PDF ...`
> - Searched the web for `site:ti.com/lit/ds/sluse13 BQ76952 datasheet Rev B ...`
>
> </details>
>
> - Ran `Get-Command pdftotext,pdftoppm,pdfinfo,mutool,magick -ErrorAction SilentlyContinue | Select-Object Name,Source`
>
> - Ran `wsl -e bash -lc "command -v pdftotext || command -v mutool || command -v python3"`
>
> - Ran `python -c "import importlib.util; print({m: bool(importlib.util.find_spec(m)) for m in ['fitz','pypdf','PyPDF2','pdfplumber','openpyxl','PIL']})"`
>
> - Ran `$paths = @('C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe','C:\Program Files\Microsoft\Edge\Application\msedge.exe','C:\Program Files\Google\Chrome\Application\chrome.exe'); $paths | Where-Object { Test-Path -LiteralPath $_ }`
>
> - Ran `& 'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe' --headless=new --disable-gpu --run-all-compositor-stages-before-draw --virtual-time-budget=5000 --screenshot='D:\Linux\project\bms\schematic_page1.png' --window-size=2400,1600 'file:///D:/Linux/project/bms/Data_BMS_10S_V1.0_280426/Data_BMS_10S_V1.0_280426/2.%20Schematic/SCH_BMS_10S_V1.0.PDF#page=1&zoom=100'`
>
> - Ran `Get-Item -LiteralPath 'D:\Linux\project\bms\schematic_page1.png' | Select-Object Length,FullName`
>
> - Ran `& 'C:\Program Files\Google\Chrome\Application\chrome.exe' --headless=new --disable-gpu --no-first-run --user-data-dir='D:\Linux\project\bms\.chrome-headless' --run-all-compositor-stages-before-draw --virtual-time-budget=5000 --screenshot='D:\Linux\project\bms\schematic_page1.png' --window-size=2400,1600 'file:///D:/Linux/project/bms/Data_BMS_10S_V1.0_280426/Data_BMS_10S_V1.0_280426/2.%20Schematic/SCH_BMS_10S_V1.0.PDF#page=1&zoom=100'`
>
> - Ran `Get-ChildItem -Force -LiteralPath 'D:\Linux\project\bms' | Where-Object { $_.Name -like '*headless*' -or $_.Name -like 'schematic*' } | Select-Object Name,Length`
>
> - Ran `Get-ChildItem -Force -LiteralPath 'D:\Linux\project\bms\.chrome-headless' | Select-Object Name,Length`
>
> - Ran `$args = @('--headless','--disable-gpu','--no-first-run','--user-data-dir=D:\Linux\project\bms\.chrome-headless','--screenshot=D:\Linux\project\bms\schematic_page1.png','--window-size=2400,1600','file:///D:/Linux/project/bms/Data_BMS_10S_V1.0_280426/Data_BMS_10S_V1.0_280426/2.%20Schematic/SCH_BMS_10S_V1.0.PDF'); $p = Start-Process -FilePath 'C:\Program Files\Google\Chrome\Application\chrome.exe' -ArgumentList $args -WindowStyle Hidden -Wait -PassThru; $p.ExitCode`
>
> - Ran `Select-String -Path 'Data_BMS_10S_V1.0_280426\Data_BMS_10S_V1.0_280426\2. Schematic\SCH_BMS_10S_V1.0.PDF' -Pattern 'BQ76952','REG1','LM5163' -SimpleMatch`
>
> </details>
>
> Mình đã xác nhận bộ thiết kế có schematic PDF và BOM, còn datasheet/TRM sẽ đối chiếu bản chính thức TI (SLUSE13B và SLUUBY2B). Một điểm đáng chú ý ban đầu: SHUTDOWN và DEEPSLEEP không chỉ khác ở LDO 5 V; trạng thái các chân sense/wake và tải được cấp từ từng cell có thể làm dòng tổng của pack cao hơn rất nhiều so với riêng dòng IQ của IC. Mình đang bóc đúng các nhánh đó khỏi mạch.
>
> <details><summary>Ran 8 commands, searched the web</summary>
>
> <details><summary>Searched the web</summary>
>
> - Searched the web for `'SHUTDOWN mode supply current'`
>
> </details>
>
> - Ran `python -c "import re,zlib,pathlib,collections; p=pathlib.Path(r'D:\Linux\project\bms\Data_BMS_10S_V1.0_280426\Data_BMS_10S_V1.0_280426\2. Schematic\SCH_BMS_10S_V1.0.PDF'); d=p.read_bytes(); ss=re.findall(rb'stream\r?\n(.*?)\r?\nendstream',d,re.S); ok=[]; bad=0; [(ok.append(zlib.decompress(s)) if not bad else None) for s in []]; print('bytes',len(d),'page_objs',len(re.findall(rb'/Type\s*/Page\b',d)),'streams',len(ss)); c=collections.Counter(); dec=[]; [dec.append(zlib.decompress(s)) if s[:2] in (b'x\x9c',b'x\xda',b'x\x01') else None for s in ss]; print('zlib',len(dec),'sizes',sum(map(len,dec))); print('hits',[(k,sum(k in x for x in dec)) for k in [b'BQ76952',b'TS2',b'REG1',b'LM5163',b'LD']])"`
>
> - Ran `python -c "import re,zlib,pathlib; d=pathlib.Path(r'D:\Linux\project\bms\Data_BMS_10S_V1.0_280426\Data_BMS_10S_V1.0_280426\2. Schematic\SCH_BMS_10S_V1.0.PDF').read_bytes(); ss=re.findall(rb'stream\r?\n(.*?)\r?\nendstream',d,re.S); dec=[zlib.decompress(s) for s in ss if s[:2] in (b'x\x9c',b'x\xda',b'x\x01')]; [(print('\nSTREAM',i,'\n',x[max(0,x.find(b\"TS2\")-1500):x.find(b\"TS2\")+3000].decode('latin1','replace'))) for i,x in enumerate(dec) if b'TS2' in x]"`
>
> - Ran `python -c "import re,zlib,pathlib; d=pathlib.Path(r'D:\Linux\project\bms\Data_BMS_10S_V1.0_280426\Data_BMS_10S_V1.0_280426\2. Schematic\SCH_BMS_10S_V1.0.PDF').read_bytes(); ss=re.findall(rb'stream\r?\n(.*?)\r?\nendstream',d,re.S); dec=[zlib.decompress(s) for s in ss if s[:2] in (bytes.fromhex('789c'),bytes.fromhex('78da'),bytes.fromhex('7801'))]; needle=bytes.fromhex('545332'); [(print('STREAM',i,x[max(0,x.find(needle)-1500):x.find(needle)+3000].decode('latin1','replace'))) for i,x in enumerate(dec) if needle in x]"`
>
> - Ran `python -c "import re,zlib,pathlib; d=pathlib.Path(r'D:\Linux\project\bms\Data_BMS_10S_V1.0_280426\Data_BMS_10S_V1.0_280426\2. Schematic\SCH_BMS_10S_V1.0.PDF').read_bytes(); ss=re.findall(rb'stream\r?\n(.*?)\r?\nendstream',d,re.S); dec=[zlib.decompress(s) for s in ss if s[:2] in (bytes.fromhex('789c'),bytes.fromhex('78da'),bytes.fromhex('7801'))]; print(dec[3][:12000].decode('latin1','replace'))"`
>
> - Ran `python -c "import re,zlib,pathlib; d=pathlib.Path(r'D:\Linux\project\bms\Data_BMS_10S_V1.0_280426\Data_BMS_10S_V1.0_280426\2. Schematic\SCH_BMS_10S_V1.0.PDF').read_bytes(); ss=re.findall(rb'stream\r?\n(.*?)\r?\nendstream',d,re.S); dec=[zlib.decompress(s) for s in ss if s[:2] in (bytes.fromhex('789c'),bytes.fromhex('78da'),bytes.fromhex('7801'))]; needle=b'BQ76952'; [(print('STREAM',i,'POS',x.find(needle),x[x.find(needle)-1200:x.find(needle)+2500].decode('latin1','replace'))) for i,x in enumerate(dec) if needle in x]"`
>
> - Ran `python -c "import re,zlib,pathlib,codecs; d=pathlib.Path(r'D:\Linux\project\bms\Data_BMS_10S_V1.0_280426\Data_BMS_10S_V1.0_280426\2. Schematic\SCH_BMS_10S_V1.0.PDF').read_bytes(); ss=re.findall(rb'stream\r?\n(.*?)\r?\nendstream',d,re.S); a=[zlib.decompress(s) for s in ss if s[:2] in (bytes.fromhex('789c'),bytes.fromhex('78da'),bytes.fromhex('7801'))][2]; bs=re.findall(rb'BT(.*?)ET',a,re.S); rows=[]; [(rows.append((float((m:=re.search(rb'[-+0-9.]+\s+[-+0-9.]+\s+[-+0-9.]+\s+[-+0-9.]+\s+([-+0-9.]+)\s+([-+0-9.]+)\s+Tm',b)).group(1)),float(m.group(2)),''.join(codecs.decode(t,'unicode_escape') for t in re.findall(rb'\(((?:\\.|[^\\()])*)\)\s*Tj',b)).replace('\x00',''))) if m and re.search(rb'\(((?:\\.|[^\\()])*)\)\s*Tj',b) else None) for b in bs]; [print(f'{y:8.2f} {x:8.2f} {t}') for x,y,t in sorted(rows,key=lambda q:(-q[1],q[0])) if t.strip()]"`
>
> - Ran `python -c "import re,zlib,pathlib,codecs; d=pathlib.Path(r'D:\Linux\project\bms\Data_BMS_10S_V1.0_280426\Data_BMS_10S_V1.0_280426\2. Schematic\SCH_BMS_10S_V1.0.PDF').read_bytes(); ss=re.findall(rb'stream\r?\n(.*?)\r?\nendstream',d,re.S); a=[zlib.decompress(s) for s in ss if s[:2] in (bytes.fromhex('789c'),bytes.fromhex('78da'),bytes.fromhex('7801'))][2]; bs=re.findall(rb'BT(.*?)ET',a,re.S); mm=[(b,re.search(rb'[-+0-9.]+\s+[-+0-9.]+\s+[-+0-9.]+\s+[-+0-9.]+\s+([-+0-9.]+)\s+([-+0-9.]+)\s+Tm',b)) for b in bs]; rows=[(float(m.group(1)),float(m.group(2)),''.join(codecs.decode(t,'unicode_escape') for t in re.findall(rb'\(((?:\\.|[^\\()])*)\)\s*Tj',b)).replace('\x00','')) for b,m in mm if m]; [print(f'{y:8.2f} {x:8.2f} {t}') for x,y,t in sorted(rows,key=lambda q:(-q[1],q[0])) if t.strip()]"`
>
> - Ran `$args = @('--headless=new','--disable-gpu','--no-first-run','--user-data-dir=D:\Linux\project\bms\.chrome-headless','--run-all-compositor-stages-before-draw','--virtual-time-budget=5000','--screenshot=D:\Linux\project\bms\schematic_page1.png','--window-size=2400,1600','file:///D:/Linux/project/bms/Data_BMS_10S_V1.0_280426/Data_BMS_10S_V1.0_280426/2.%20Schematic/SCH_BMS_10S_V1.0.PDF#page=1&zoom=100'); $p = Start-Process -FilePath 'C:\Program Files\Google\Chrome\Application\chrome.exe' -ArgumentList $args -WindowStyle Hidden -Wait -PassThru; $p.ExitCode; Get-Item -LiteralPath 'D:\Linux\project\bms\schematic_page1.png' | Select-Object Length,FullName`
>
> </details>
>
> Viewed an image
>
> <details><summary>Ran a command</summary>
>
> - Ran `$args = @('--headless=new','--disable-gpu','--no-first-run','--allow-file-access-from-files','--user-data-dir=D:\Linux\project\bms\.chrome-headless2','--run-all-compositor-stages-before-draw','--virtual-time-budget=15000','--screenshot=D:\Linux\project\bms\schematic_page3.png','--window-size=3000,2200','file:///D:/Linux/project/bms/Data_BMS_10S_V1.0_280426/Data_BMS_10S_V1.0_280426/2.%20Schematic/SCH_BMS_10S_V1.0.PDF#page=3&zoom=page-width'); $p = Start-Process -FilePath 'C:\Program Files\Google\Chrome\Application\chrome.exe' -ArgumentList $args -WindowStyle Hidden -Wait -PassThru; $p.ExitCode; Get-Item -LiteralPath 'D:\Linux\project\bms\schematic_page3.png' | Select-Object Length,FullName`
>
> </details>
>
> Viewed an image
>
> <details><summary>Ran 7 commands</summary>
>
> - Ran `Get-ChildItem -Path 'C:\Program Files','C:\Program Files (x86)' -Recurse -File -ErrorAction SilentlyContinue -Include pdftoppm.exe,mutool.exe,gswin64c.exe,gswin32c.exe,SumatraPDF.exe,Acrobat.exe,AcroRd32.exe | Select-Object FullName`
> - Ran `python -m pip install --target C:\Users\admin\AppData\Local\Temp\codex_pymupdf pymupdf==1.26.3`
> - Ran `$env:PYTHONPATH='C:\Users\admin\AppData\Local\Temp\codex_pymupdf'; python -c "import fitz; p=r'D:\Linux\project\bms\Data_BMS_10S_V1.0_280426\Data_BMS_10S_V1.0_280426\2. Schematic\SCH_BMS_10S_V1.0.PDF'; d=fitz.open(p); print('pages',d.page_count); [(print(i+1,d[i].rect,len(d[i].get_text()),'BQ76952' in d[i].get_text(),d[i].get_text()[:120].replace(chr(10),' | '))) for i in range(d.page_count)]"`
> - Ran `$env:PYTHONPATH='C:\Users\admin\AppData\Local\Temp\codex_pymupdf'; python -c "import pymupdf; p=r'D:\Linux\project\bms\Data_BMS_10S_V1.0_280426\Data_BMS_10S_V1.0_280426\2. Schematic\SCH_BMS_10S_V1.0.PDF'; d=pymupdf.open(p); print('pages',d.page_count); [(print(i+1,d[i].rect,len(d[i].get_text()),'BQ76952' in d[i].get_text(),d[i].get_text()[:120].replace(chr(10),' | '))) for i in range(d.page_count)]"`
> - Ran `$env:PYTHONPATH='C:\Users\admin\AppData\Local\Temp\codex_pymupdf'; python -c "import pymupdf,sys; print(pymupdf, getattr(pymupdf,'__file__',None)); print(sys.path[:5]); print(dir(pymupdf)[:30])"`
> - Ran `Get-ChildItem -LiteralPath 'C:\Users\admin\AppData\Local\Temp\codex_pymupdf\pymupdf' -Force | Select-Object Name,Length,Mode`
> - Ran `$env:PYTHONPATH='C:\Users\admin\AppData\Local\Temp\codex_pymupdf'; python -c "import pymupdf; p=r'D:\Linux\project\bms\Data_BMS_10S_V1.0_280426\Data_BMS_10S_V1.0_280426\2. Schematic\SCH_BMS_10S_V1.0.PDF'; d=pymupdf.open(p); print('pages',d.page_count); [(print(i+1,d[i].rect,len(d[i].get_text()),'BQ76952' in d[i].get_text())) for i in range(d.page_count)]; page=[i for i in range(d.page_count) if 'BQ76952' in d[i].get_text()][0]; pix=d[page].get_pixmap(matrix=pymupdf.Matrix(2.5,2.5),alpha=False); pix.save(r'D:\Linux\project\bms\schematic_bq_page.png'); print('rendered',page+1,pix.width,pix.height)"`
>
> </details>
>
> Viewed an image
>
> <details><summary>Ran a command</summary>
>
> - Ran `$env:PYTHONPATH='C:\Users\admin\AppData\Local\Temp\codex_pymupdf'; python -c "import pymupdf; p=r'D:\Linux\project\bms\Data_BMS_10S_V1.0_280426\Data_BMS_10S_V1.0_280426\2. Schematic\SCH_BMS_10S_V1.0.PDF'; d=pymupdf.open(p); clip=pymupdf.Rect(560,260,1100,740); pix=d[1].get_pixmap(matrix=pymupdf.Matrix(4,4),clip=clip,alpha=False); pix.save(r'D:\Linux\project\bms\schematic_bq_crop.png'); print(pix.width,pix.height)"`
>
> </details>
>
> Viewed an image
>
> <details><summary>Ran a command</summary>
>
> - Ran `$env:PYTHONPATH='C:\Users\admin\AppData\Local\Temp\codex_pymupdf'; python -c "import pymupdf; p=r'D:\Linux\project\bms\Data_BMS_10S_V1.0_280426\Data_BMS_10S_V1.0_280426\2. Schematic\SCH_BMS_10S_V1.0.PDF'; d=pymupdf.open(p); [(d[i].get_pixmap(matrix=pymupdf.Matrix(2.5,2.5),alpha=False).save(fr'D:\Linux\project\bms\schematic_p{i+1}.png')) for i in range(d.page_count)]; print([(i+1,d[i].get_text()[:80].replace(chr(10),' | ')) for i in range(d.page_count)])"`
>
> </details>
>
> Viewed 2 images
>
> Viewed 2 images
>
> <details><summary>Ran a command</summary>
>
> - Ran `$env:PYTHONPATH='C:\Users\admin\AppData\Local\Temp\codex_pymupdf'; python -c "import pymupdf; p=r'D:\Linux\project\bms\Data_BMS_10S_V1.0_280426\Data_BMS_10S_V1.0_280426\2. Schematic\SCH_BMS_10S_V1.0.PDF'; d=pymupdf.open(p); clip=pymupdf.Rect(550,180,1450,950); pix=d[4].get_pixmap(matrix=pymupdf.Matrix(3.5,3.5),clip=clip,alpha=False); pix.save(r'D:\Linux\project\bms\schematic_fet_crop.png'); print(pix.width,pix.height)"`
>
> </details>
>
> Viewed an image
>
> <details><summary>Ran 2 commands, searched the web</summary>
>
> - Ran `rg -n -C 5 "DEEPSLEEP|SHUTDOWN|DPSLP|REG12|Power Config|REG2|bq76952_enter|SD_CMD|BatteryStatus|SHUT_TS2|RST_SHUT" BMS/MyDrivers/bq76952 BMS/MyDrivers/power BMS/App BMS/Core/Src/main.c BMS/build/.build_config`
>
> - Ran `Get-Content -Path 'BMS\MyDrivers\bq76952\bq76952.c' | Select-Object -Skip 455 -First 65`
>
> <details><summary>Searched the web</summary>
>
> - Searched the web for `'If the voltage on the LD pin is still above'`
>
> </details>
>
> </details>
>
> <details><summary>Ran 5 commands, searched the web</summary>
>
> - Ran `rg -n -C 6 "Disable_Power_Battery|Enable_Power_Battery|BATS_EN|POW_EN|GPIO.*ANALOG|STOP|STANDBY|EnterSTOP|enter_low_power|SHUT_Pin|BAT_EN" BMS`
>
> <details><summary>Searched the web</summary>
>
> - Searched the web for `site:ti.com/lit/ds TLV757P quiescent current datasheet ...`
> - Searched the web for `'Ground current'`
>
> </details>
>
> - Ran `rg -n "MAINAPP_SLEEP_WAKEUP_MS|MAINAPP_SLEEP_TIMEOUT|SLEEP_WAKEUP|POWER_MANAGER.*RTC|auto_wakeup_ms|RTC_WakeUp|LPTIM" BMS/App BMS/MyDrivers/power BMS/Core/Src/rtc.c BMS/Core/Src/lptim.c`
>
> - Ran `Get-Content -Path 'BMS\App\mainapp.c' | Select-Object -Skip 185 -First 80`
>
> - Ran `Get-Content -Path 'BMS\MyDrivers\power\power_manager.c' | Select-Object -Skip 200 -First 285`
>
> <details><summary>Searched the web</summary>
>
> - Searched the web for `'VWAKEONLD'`
> - Searched the web for `site:e2e.ti.com BQ76952 "70 uA" shutdown ...`
> - Searched the web for `'IQ Quiescent current'`
>
> </details>
>
> - Ran `$env:PYTHONPATH='C:\Users\admin\AppData\Local\Temp\codex_pymupdf'; python -c "import pymupdf; p=r'D:\Linux\project\bms\Data_BMS_10S_V1.0_280426\Data_BMS_10S_V1.0_280426\2. Schematic\SCH_BMS_10S_V1.0.PDF'; d=pymupdf.open(p); clip=pymupdf.Rect(1180,150,1510,520); pix=d[1].get_pixmap(matrix=pymupdf.Matrix(5,5),clip=clip,alpha=False); pix.save(r'D:\Linux\project\bms\schematic_batsense_crop.png'); print(pix.width,pix.height)"`
>
> </details>
>
> Viewed an image
>
> <details><summary>Ran 4 commands, searched the web</summary>
>
> <details><summary>Searched the web</summary>
>
> - Searched the web for `'0x92FA'`
>
> </details>
>
> - Ran `Get-Content -Path 'BMS\MyDrivers\bq76952\bq76952.c' | Select-Object -Skip 1585 -First 75; Get-Content -Path 'BMS\MyMiddlewares\bms\bms.c' | Select-Object -Skip 350 -First 40`
>
> - Ran `Get-Content -Path 'BMS\MyDrivers\bq76952\bq76952.c' | Select-Object -Skip 24 -First 40; Get-Content -Path 'BMS\MyDrivers\bq76952\bq76952.c' | Select-Object -Skip 288 -First 45; Get-Content -Path 'BMS\MyDrivers\bq76952\bq76952.h' | Select-Object -Skip 112 -First 22`
>
> - Ran `rg -n -C 8 "BMS_SetBatSenseEnable" BMS/MyMiddlewares/bms/bms.c`
>
> - Ran `rg -n "BMS_BAT_ADC_SAMPLE_MS|BMS_BAT_ADC_SETTLE_MS" BMS`
>
> <details><summary>Searched the web</summary>
>
> - Searched the web for `'10 MΩ resistor from BREG'`
>
> </details>
>
> </details>
>
> Context compaction
>
> Source: automatic
> Status: completed
>
> Mình đang chốt lại theo hai hướng tách biệt: “BQ có thật sự vào shutdown sâu hay chỉ soft-shutdown” và “nhánh nào trên bo tạo đúng mức 70/115 µA”. Phép tính nhánh BAT_ADC đang cho kết quả trùng gần như tuyệt đối với số đo 70 µA, nên mình sẽ ưu tiên một chuỗi đo có thể xác nhận hoặc loại trừ nó ngay trên bo.
>
> <details><summary>Ran 21 commands, searched the web</summary>
>
> - Ran `git status --short`
>
> - Ran `rg -n "#define (DEEPSLEEP|SHUTDOWN)|USER_DEFS|DPSLP|SHUTDOWN_PENDING|CFETOFF_PIN_CONFIG|DFETOFF_PIN_CONFIG|BATS_EN|POW_EN|0x000F|0x0010|0x009F" BMS`
>
> - Ran `Get-ChildItem -Force | Where-Object { $_.Name -match '^(schematic_|\.chrome-headless)' } | Select-Object Name,Length,Mode`
>
> <details><summary>Searched the web</summary>
>
> - Searched the web for `https://www.ti.com/lit/ds/symlink/bq76952.pdf`
> - Searched the web for `'DEEPSLEEP mode, LFO disabled'`
> - Searched the web for `'VWAKEONLD'`
>
> </details>
>
> - Ran `Get-Content 'BMS/MyMiddlewares/bms/bms.c' | Select-Object -Skip 559 -First 61`
>
> - Ran `Get-Content 'BMS/MyDrivers/bq76952/bq76952.c' | Select-Object -Skip 1434 -First 131`
>
> - Ran `Get-Content 'BMS/MyApp/mainapp.c' | Select-Object -Skip 189 -First 61`
>
> - Ran `Get-Content 'BMS/MyDrivers/bq76952/bq76952.c' | Select-Object -Skip 449 -First 71`
>
> - Ran `Get-Content 'BMS/MyDrivers/bq76952/bq76952.c' | Select-Object -Skip 1579 -First 51`
>
> - Ran `Get-Content 'BMS/MyDrivers/power/power_manager.c' | Select-Object -Skip 239 -First 61`
>
> - Ran `Get-Content 'BMS/MyApp/mainapp.c' | Select-Object -Skip 0 -First 40`
>
> - Ran `Get-Content 'BMS/MyDrivers/bq76952/bq76952.h' | Select-Object -Skip 0 -First 155`
>
> - Ran `Get-Content 'BMS/MyMiddlewares/bms/bms.c' | Select-Object -Skip 429 -First 61`
>
> - Ran `Get-Content 'BMS/MyDrivers/bq76952/bq76952.h' | Select-Object -Skip 0 -First 155`
>
> - Ran `Get-Content 'BMS/MyDrivers/bq76952/bq76952.c' | Select-Object -Skip 449 -First 71`
>
> - Ran `Get-Content 'BMS/MyDrivers/bq76952/bq76952.c' | Select-Object -Skip 1434 -First 131`
>
> - Ran `Get-Content 'BMS/MyDrivers/bq76952/bq76952.c' | Select-Object -Skip 1579 -First 51`
>
> - Ran `Get-Content 'BMS/MyDrivers/power/power_manager.c' | Select-Object -Skip 239 -First 61`
>
> - Ran `Get-Content 'BMS/MyMiddlewares/bms/bms.c' | Select-Object -Skip 429 -First 61`
>
> - Ran `Get-Content 'BMS/MyMiddlewares/bms/bms.c' | Select-Object -Skip 559 -First 61`
>
> - Ran `Get-Content 'BMS/App/mainapp.c' | Select-Object -Skip 0 -First 40`
>
> - Ran `Get-Content 'BMS/App/mainapp.c' | Select-Object -Skip 164 -First 76`
>
> </details>
>
> Viewed an image
>
> Viewed an image
>
> Viewed an image
>
> Viewed an image
>
> Viewed an image
>
> Viewed an image
>
> Viewed an image
>
> <details><summary>Ran 2 commands, searched the web</summary>
>
> <details><summary>Searched the web</summary>
>
> - Searched the web for `'LOAD_DETECT_ON'`
> - Searched the web for `site:ti.com/lit/ds/symlink/tlv757p.pdf TLV757P ground current 25 uA ...`
>
> </details>
>
> - Ran `rg -n -C 8 "bq76952_applyReg12Control|bq76952_applySleepRegulatorControl|bq76952_setEnableRegulator|BQ_REG12" BMS/MyDrivers/bq76952/bq76952.c`
>
> - Ran `rg -n "SLEEP_KEEP|REG1_VOLTAGE|REG2_VOLTAGE|REG12_CONFIG|ALERT_PIN_CONFIG_OPEN" BMS/MyDrivers/bq76952`
>
> </details>
>
> <details><summary>Ran 2 commands, searched the web</summary>
>
> - Ran `git diff -- BMS/MyDrivers/bq76952/bq76952.c BMS/MyMiddlewares/bms/bms.c`
>
> <details><summary>Searched the web</summary>
>
> - Searched the web for `'0x92FA CFETOFF Pin Config'`
>
> </details>
>
> - Ran `Get-Content 'BMS/build/.build_config'`
>
> </details>
>
> Mình đã hoàn tất đối chiếu sơ đồ với firmware. Có một chi tiết quan trọng: net ghi “TS2” trên sơ đồ thực ra đi vào chân TS3 vật lý; chân TS2 có chức năng wake/shutdown là net `WAKE`. Mình cũng đang dọn các ảnh tạm đã tạo khi phóng to PDF, không đụng vào các thay đổi source hiện có của bạn.
>
> <details><summary>Ran 16 commands</summary>
>
> - Ran `Resolve-Path -LiteralPath '.chrome-headless2' | Select-Object -ExpandProperty Path`
> - Ran `Resolve-Path -LiteralPath 'schematic_p5.png' | Select-Object -ExpandProperty Path`
> - Ran `Resolve-Path -LiteralPath 'schematic_p4.png' | Select-Object -ExpandProperty Path`
> - Ran `Resolve-Path -LiteralPath 'schematic_fet_crop.png' | Select-Object -ExpandProperty Path`
> - Ran `Resolve-Path -LiteralPath 'schematic_batsense_crop.png' | Select-Object -ExpandProperty Path`
> - Ran `Resolve-Path -LiteralPath 'schematic_bq_crop.png' | Select-Object -ExpandProperty Path`
> - Ran `Resolve-Path -LiteralPath 'schematic_p1.png' | Select-Object -ExpandProperty Path`
> - Ran `Resolve-Path -LiteralPath '.chrome-headless' | Select-Object -ExpandProperty Path`
> - Ran `Resolve-Path -LiteralPath 'schematic_page1.png' | Select-Object -ExpandProperty Path`
> - Ran `Resolve-Path -LiteralPath 'schematic_bq_page.png' | Select-Object -ExpandProperty Path`
> - Ran `Resolve-Path -LiteralPath 'schematic_p3.png' | Select-Object -ExpandProperty Path`
> - Ran `Resolve-Path -LiteralPath 'schematic_page3.png' | Select-Object -ExpandProperty Path`
> - Ran `Resolve-Path -LiteralPath 'schematic_p2.png' | Select-Object -ExpandProperty Path`
> - Ran `Remove-Item -LiteralPath 'D:\Linux\project\bms\schematic_batsense_crop.png','D:\Linux\project\bms\schematic_bq_crop.png','D:\Linux\project\bms\schematic_bq_page.png','D:\Linux\project\bms\schematic_fet_crop.png','D:\Linux\project\bms\schematic_p1.png','D:\Linux\project\bms\schematic_p2.png','D:\Linux\project\bms\schematic_p3.png','D:\Linux\project\bms\schematic_p4.png','D:\Linux\project\bms\schematic_p5.png','D:\Linux\project\bms\schematic_page1.png','D:\Linux\project\bms\schematic_page3.png' -Force`
> - Ran `Remove-Item -LiteralPath 'D:\Linux\project\bms\.chrome-headless' -Recurse -Force`
> - Ran `Remove-Item -LiteralPath 'D:\Linux\project\bms\.chrome-headless2' -Recurse -Force`
>
> </details>
>
> <details><summary>Ran a command</summary>
>
> - Ran `git status --short`
>
> </details>
>
> <details><summary>Plan</summary>
>
> Đã đối chiếu datasheet/TRM chính thức, sơ đồ 5 trang, BOM và firmware hiện tại; đã lập ngân sách dòng và chuỗi phép đo cô lập nguyên nhân.
>
> - [x] Locate and extract the relevant datasheets, reference manual, schematic, and BOM
> - [x] Establish the BQ76952 low-power specifications and mode-entry conditions
> - [x] Trace every plausible board-level leakage path in shutdown and deep sleep
> - [x] Synthesize a ranked root-cause analysis and decisive measurement plan
>
> </details>
</details>

## Kết luận

Nguyên nhân có xác suất cao nhất là nhánh đo `BAT_ADC` vẫn đang bật khi vào low-power. Giá trị của nhánh này khớp gần như chính xác với 70 µA. Mức 115 µA ở DEEPSLEEP hợp lý khi cộng thêm dòng của BQ, LDO TLV75733 và STM32 đang STOP.

### 1. “Chữ ký” 70 µA của nhánh BAT_ADC

Trong [sơ đồ nguyên lý, trang 2](<./Data_BMS_10S_V1.0_280426/Data_BMS_10S_V1.0_280426/2. Schematic/SCH_BMS_10S_V1.0.PDF>):

- Nhánh đo: `Cell10 → Q17 → R86 665k → R85 13.3k → GND`.
- Nhánh điều khiển: `Cell10 → R92 1M → R91 3.3M → Q18 → GND`.
- Gate Q18 nối trực tiếp `BATS_EN`/PB4, không có điện trở kéo xuống ngoài.

Khi Q18 dẫn:

\[
I=V_{pack}\left(\frac{1}{678.3k}+\frac{1}{4.3M}\right)
\]

| Điện áp pack | Dòng tính được |
|---:|---:|
| 36 V | 61.45 µA |
| 40 V | 68.28 µA |
| 41 V | **70.0 µA** |
| 42 V | 71.69 µA |

Đây gần như chính xác số đo của bạn. Khi nhánh bật, điện áp `BAT_ADC` phải khoảng 0.71–0.82 V; tại 41 V là khoảng 0.804 V.

Firmware hiện tại có kéo `BATS_EN` xuống trước STOP tại [power_manager.c](./BMS/MyDrivers/power/power_manager.c:267), nhưng thao tác này xảy ra sau khi đã gửi lệnh low-power cho BQ. Khi REG2/MCU mất nguồn, gate Q18 không có điện trở kéo xuống phần cứng nên trạng thái thực tế cần được đo, không nên chỉ dựa vào code.

Việc tháo BQ rồi dòng về 0 vẫn phù hợp với giả thuyết này: tháo BQ làm mất nguồn MCU, khiến Q18 không còn được điều khiển bật. Nó chưa chứng minh bản thân BQ tiêu thụ 70 µA.

### 2. Vì sao DEEPSLEEP là 115 µA

Datasheet chỉ công bố:

- DEEPSLEEP, LFO off, REG2 off: 9.2 µA điển hình.
- DEEPSLEEP, LFO on, REG2 off: 10.7 µA.
- True SHUTDOWN: 1 µA điển hình, 3.1 µA tối đa.

Các điều kiện này nằm ở mục 7.5 của [datasheet BQ76952 Rev. B](https://www.ti.com/lit/ds/symlink/bq76952.pdf).

Trong firmware hiện tại:

- Chế độ mặc định là DEEPSLEEP tại [bq76952.h](./BMS/MyDrivers/bq76952/bq76952.h:17).
- `DPSLP_LDO=1`, REG1 tắt nhưng **REG2 5 V vẫn bật** tại [bq76952.c](./BMS/MyDrivers/bq76952/bq76952.c:1453) và [bq76952.c](./BMS/MyDrivers/bq76952/bq76952.c:1481).
- REG2 cấp U2 TLV75733 rồi cấp STM32.
- TLV75733 tự tiêu thụ khoảng 25 µA điển hình; STM32L010 STOP+RTC khoảng dưới 1 µA điển hình. [TLV757P](https://www.ti.com/product/TLV757P), [STM32L010C6 datasheet](https://www.st.com/resource/en/datasheet/stm32l010c6.pdf).

Do đó:

\[
70\ \mu A\;(BAT\_ADC)
+ 9\text{–}11\ \mu A\;(BQ)
+ 25\ \mu A\;(U2)
+ MCU + REG0 + rò
\approx 105\text{–}115\ \mu A
\]

Mức tăng từ 70 lên 115 µA là 45 µA, rất phù hợp với tải nguồn REG2.

Nếu chỉ không hàn U5 nhưng R64=63.4k và R65=200k vẫn còn, riêng cầu chia áp LM5163 còn tiêu thụ:

\[
5/(63.4k+200k)\approx19\ \mu A
\]

Nếu toàn bộ khối LM5163 đều DNP thì bỏ qua phần này.

### 3. Phải phân biệt true shutdown và soft shutdown

BQ có thể mắc ở soft shutdown nếu:

- Chân TS2 vật lý bị kéo thấp.
- `LD` còn cao hơn ngưỡng wake.
- `Power Config.SHUT_TS2`, bit 12, bằng 1.

Khi đó FET và đo lường đã tắt, thậm chí 5 V cũng có thể tắt trong TS2-soft-shutdown. Vì vậy “5 V off” hoặc I²C NACK không đủ chứng minh true shutdown. Cách xác định chắc nhất là:

- `REG18 = 0 V`: true shutdown.
- `REG18 ≈ 1.8 V`: soft shutdown/low-power state, chưa phải true shutdown.

TI mô tả chi tiết tại [TRM BQ76952, mục 7.4–7.5](https://www.ti.com/lit/ug/sluuby2b/sluuby2b.pdf) và [TI FAQ về Soft SHUTDOWN](https://e2e.ti.com/support/power-management-group/power-management/f/power-management-forum/1273427/faq-bq76952-what-is-the-soft-shutdown-state).

Đáng chú ý trên sơ đồ:

- TS2 vật lý là U1 pin 22, net `WAKE`.
- Net có tên `TS2` trên bản vẽ lại nối vào pin 23, tức TS3 vật lý.
- Vì vậy RT2 không phải linh kiện cản shutdown.
- Cần kiểm tra `WAKE`, S1, flux/ẩm/bẩn PCB. TI cho biết chỉ khoảng 1 µA rò tại TS2 cũng có thể giữ soft shutdown.
- TS2 được kéo lên qua khoảng 4.6 MΩ nên DMM 10 MΩ có thể chỉ đọc khoảng 3.4 V thay vì 5 V.

`LD` nối PACK+ qua R138 và R141‖R142. Hãy tháo charger/load và bảo đảm `LD < 0.8 V` trước khi gửi shutdown.

Firmware hiện cũng coi shutdown “đã bắt đầu” ngay khi I²C mất kết nối hoặc `SHUTDOWN_PENDING` được set tại [bq76952.c](./BMS/MyDrivers/bq76952/bq76952.c:475). Đây không phải xác nhận true shutdown.

### 4. Phép đo quyết định

| REG18 | BAT_ADC | Kết luận |
|---:|---:|---|
| 0 V | 0.71–0.82 V | True shutdown nhưng nhánh BAT_ADC vẫn bật |
| 1.8 V | 0.71–0.82 V | Vừa soft shutdown, vừa bật BAT_ADC |
| 1.8 V | ≈0 V | Soft shutdown do WAKE/LD/SHUT_TS2 |
| 0 V | ≈0 V | Kiểm tra BREG/Q19 hoặc rò/hỏng BQ |

Thứ tự thử nên là:

1. Đo `REG18`, `BAT_ADC`, `BATS_EN`, gate Q18, `WAKE` và `LD`.
2. Kéo tạm gate Q18 xuống GND bằng 100 kΩ–1 MΩ, hoặc nhấc R91/Q18. Nếu dòng giảm 62–72 µA thì đã xác nhận nguyên nhân chính.
3. Khi DEEPSLEEP, sau khi đã cưỡng bức `BATS_EN=0`, nhấc D28 để tách toàn bộ U2/MCU khỏi REG2.
4. Đọc `Power Config 0x9234`: xác nhận bit 12 `SHUT_TS2=0`, bit 10 `DPSLP_LDO`, bit 9 `DPSLP_LFO`.
5. Đọc `ControlStatus.DEEPSLEEP` để chắc chắn BQ không bị LD đánh thức trở lại.

### 5. Một lỗi phần cứng khác cần sửa

Mạch BREG/Q19 không có:

- Điện trở khoảng 10 MΩ từ BREG xuống VSS.
- Tụ BREG–VSS 150 pF.

TI đặc biệt khuyến nghị điện trở 10 MΩ để tránh dòng rò ngoài ý muốn trong SHUTDOWN. Nếu `REG18=0` và `BAT_ADC=0` nhưng dòng vẫn cao, hãy đo BREG/REGIN, gắn thử 10 MΩ + 150 pF hoặc tạm nhấc Q19/D20. Khuyến nghị này nằm ở phần ứng dụng của [datasheet BQ76952](https://www.ti.com/lit/ds/symlink/bq76952.pdf).

Ngoài ra firmware có một số lỗi địa chỉ pin-config tại [bq76952.h](./BMS/MyDrivers/bq76952/bq76952.h:122): CFETOFF/DFETOFF đang lệch địa chỉ, và BOTHOFF dùng sai bit tại [bq76952.c](./BMS/MyDrivers/bq76952/bq76952.c:1610). Chúng không cần thiết để giải thích con số 70/115 µA, nhưng nên sửa trước khi chốt firmware.

Tóm lại: ưu tiên đo `BAT_ADC` và `REG18`. Chỉ hai giá trị này gần như sẽ xác định ngay nguyên nhân thuộc nhánh BAT_ADC, soft shutdown, hay BREG. Mình không sửa source trong lượt phân tích này.

> với nhánh BAT_ADC khi tháo BQ thì chân gate q18 vẫn tương tự khi shutdown mà nhỉ, stm32 đều k có nguồn, vậy nếu nó k được kéo xuống, thì khi stm32 off thì nó đều phải như nhau dù trước đó nó như nào chứ

Đúng — lập luận của bạn hợp lý.

Nếu trong cả hai trường hợp:

- STM32 mất nguồn hoàn toàn,
- không có đường back-power vào STM32,
- trạng thái ban đầu và thời gian chờ giống nhau,

thì gate Q18 phải có trạng thái tương đương. Vì vậy kết quả “tháo BQ → dòng 0” làm giả thuyết BAT_ADC yếu đi nếu 70 µA tồn tại ổn định lâu sau shutdown.

Điểm khác biệt có thể còn lại là:

- Khi shutdown, Q18 có thể giữ điện tích gate từ trước. MOSFET gate nổi có thể duy trì trạng thái một thời gian, nhưng không nên được xem là nguyên nhân ổn định lâu dài.
- Nếu Q18 còn dẫn, `BAT_ADC ≈ 0.7–0.82 V` có thể bơm ngược vào chân ADC của STM32 khi VDD=0 qua diode bảo vệ, làm trạng thái các chân của MCU khác với một bo đã tháo BQ và để xả hoàn toàn.
- Khi tháo BQ vật lý, thời gian thao tác và tiếp xúc đo thường làm gate/Q18 và các tụ xả hết.
- Quan trọng hơn, tháo BQ cũng loại bỏ các đường rò nội bộ của BQ, REG18, BREG/REGIN và soft-shutdown. Đây có thể mới là lý do dòng về 0.

Vì vậy phép thử đúng không phải tháo BQ, mà là cưỡng bức Q18 tắt khi BQ vẫn đang hàn:

1. Vào shutdown và chờ dòng ổn định.
2. Đo `BAT_ADC`:
   - Khoảng 0.7–0.82 V: nhánh BAT_ADC thực sự đang bật.
   - Gần 0 V: nhánh này không gây 70 µA.
3. Kéo trực tiếp gate Q18 xuống GND bằng khoảng 100 kΩ, hoặc tạm nhấc R91/Q18:
   - Dòng giảm khoảng 60–72 µA: xác nhận BAT_ADC.
   - Dòng không đổi: loại trừ BAT_ADC.

Sau đó đo `REG18`:

| REG18 | BAT_ADC | Nhận định |
|---:|---:|---|
| 1.8 V | ≈0 V | Soft shutdown, ưu tiên kiểm `WAKE/TS2`, `LD`, `SHUT_TS2` |
| 0 V | ≈0 V | True shutdown; ưu tiên kiểm đường BREG–Q19 vì sơ đồ thiếu 10 MΩ và 150 pF |
| 0 hoặc 1.8 V | 0.7–0.82 V | BAT_ADC vẫn đang bật, có thể kèm soft shutdown |

Tóm lại: con số tính toán BAT_ADC khớp 70 µA rất đáng kiểm tra, nhưng quan sát “tháo BQ → 0” khiến mình không nên kết luận BAT_ADC là nguyên nhân chắc chắn. Nếu `BAT_ADC` đo được gần 0 V trong shutdown, nghi phạm mạnh hơn sẽ là soft shutdown hoặc đường rò BREG/Q19.

> tôi vừa xác nhận, khi tháo hẳn khối ADC ra dòng thì shutdown vẫn 70uA

Thông tin này loại trừ nhánh `BAT_ADC` khỏi nguyên nhân 70 µA. Lúc này chỉ còn hai hướng chính: BQ đang ở soft shutdown, hoặc dòng rò qua mạch BREG–Q19 dù BQ đã vào true shutdown.

Phép đo quyết định đầu tiên là `REG18` tại U1 pin 24/C19:

- `REG18 ≈ 1.8 V`: BQ chưa shutdown thật.
- `REG18 ≈ 0 V`: BQ đã true shutdown; chuyển sang kiểm tra BREG.

### Nếu REG18 = 1.8 V

Đây là soft shutdown. Vì 5 V cũng đã tắt, nghi ngờ cao hơn là TS2-soft-shutdown, không phải LD-soft-shutdown.

Kiểm tra:

- TS2 vật lý là pin 22, net `WAKE`, không phải net có tên `TS2`.
- Đo `WAKE`. Do pull-up nội khoảng 4.6 MΩ, DMM 10 MΩ có thể chỉ đọc khoảng 3.4 V; nếu gần 0–1 V thì S1, flux, ẩm hoặc leakage đang kéo thấp.
- Đo `LD`; bảo đảm thấp hơn 0.8 V.
- Trước khi shutdown, đọc `Power Config 0x9234`, bảo đảm `SHUT_TS2`, bit 12, bằng 0.
- Không dùng 5 V off, I²C NACK hoặc `SHUTDOWN_PENDING` làm bằng chứng shutdown thật.

### Nếu REG18 = 0 V

Khi đó 70 µA rất có khả năng đi qua mạch preregulator:

`Cell10 → D20 → R94 → Q19 → REGIN/BQ`

Sơ đồ đang thiếu hai linh kiện TI yêu cầu/khuyến nghị:

- Tụ khoảng 150 pF từ BREG xuống VSS.
- Điện trở khoảng 10 MΩ từ BREG xuống VSS để tránh leakage trong shutdown.

Chỉ cần vài trăm nA tại base Q19, với hệ số khuếch đại của BJT, có thể tạo ra hàng chục µA ở collector; vì vậy 70 µA là hoàn toàn khả thi.

Thử theo thứ tự:

1. Gắn tạm `10 MΩ` từ BREG pin 37 xuống VSS.
2. Đo lại dòng shutdown.
3. Nếu chưa giảm, tạm nhấc D20 hoặc R94 để cắt nguồn collector Q19.
4. Nếu nhấc D20/R94 làm dòng từ 70 µA về vài µA, mạch BREG–Q19 đã được xác nhận.
5. Đo thêm:
   - `BREG` so với VSS.
   - `REGIN` so với VSS.
   - Điện áp hai đầu R94; từ đó tính trực tiếp dòng nhánh Q19.

Ví dụ:

\[
I_{Q19} \approx \frac{V_{R94}}{249\ \Omega}
\]

Nếu dòng là 70 µA thì điện áp trên R94 chỉ khoảng:

\[
70\ \mu A \times 249\ \Omega \approx 17.4\ mV
\]

Do đó nên dùng đồng hồ có thang mV tốt.

Với kết quả mới, thứ tự nghi phạm hiện tại là:

1. Soft shutdown — xác định bằng `REG18`.
2. Leakage BREG–Q19 do thiếu 10 MΩ/150 pF.
3. BQ bị hỏng hoặc PCB nhiễm bẩn/rò tại BAT, REGIN, BREG, TS2 hay LD.

Đo `REG18` trước sẽ chia đôi bài toán ngay lập tức.
