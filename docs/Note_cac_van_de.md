1: chân Cell 1,2 đọc ra giá trị âm. đo trên mạch cũng khoảng 0V (do truecj tiếp từ jack pin cắm vào)
2: chân cell 3: khi cắm 3,3v từ St-link : đọc ra 1,8-1,9V đo trên mạch tương tự 
                khi rút 3,3v từ St-link( sử dụng nguồn từ BQ) : đọc rav 3V ( thực tế khoảng 4V)
                (do truecj tiếp từ jack pin cắm vào)
3: trở nhiệt trên chân TS3: đọc ra 0.
4: dòng điện đọc ra khoảng 400mA( có thể do chưa cắm chân bat)
5: điện áp pack đọc từ BQ 31V, chân BAT_ADC, đọc giá trị ADC bị giảm dần từ khoảng 31V-9V
    thực tế điện ap do từ 2 đầu bat+- là 41V

6: khi chạy ở trạng thái bình thường ( sử dụng nguồn từ BQ ) 
    + uart cắm 3 chân tx, rx, gnd
    + st-link cắm 3 chân swdio, swclk, gnd
    -> mạch chạy bình thường ( log in ra bình thường)
    + rút gnd và swclk ra khỏi st-link -> mạch vãn chạy bình thường
    + rút chân swdio ra khỏi st-link -> mạch chạy loạn, log in ra giá trị rác, lung tung