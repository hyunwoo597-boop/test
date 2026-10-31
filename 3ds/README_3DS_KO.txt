버뮤다 신드롬 한글판 - Nintendo 3DS 포트 (New 3DS 권장)
=====================================================
엔진: cyxx/bermuda 0.1.7 (Gregory Montoir) + 3DS 전용 백엔드(libctru, SDL 미사용)
한글 폰트: 갈무리14 기반 BT16.TBM (SIL OFL)
음악: 원본 MIDI를 GeneralUser GS 사운드폰트로 렌더링한 OGG (22050Hz)

[SD카드 배치]
sdmc:/3ds/bermuda/
  bermuda.3dsx      <- 홈브류 런처용 (CIA로 설치했다면 없어도 됨)
  BT16.TBM
  BERMUDA/          <- 한글판 CD의 BERMUDA 폴더 통째로 (SCN, WGP, TEXT, DATA, DDD ...)
  MUSIC/            <- track01.ogg ~ track12.ogg (22050Hz 스테레오)
  (세이브 파일 bermuda.001 등은 이 폴더에 자동 생성)

[소리가 안 나올 때]
3DS 홈브류는 DSP 펌웨어 덤프가 필요합니다: sdmc:/3ds/dspfirm.cdc
(Luma3DS 사용 시 Rosalina 메뉴 > Miscellaneous options > Dump DSP firmware)

[화면]
위 화면 : 게임 화면 (640x480을 320x240으로 축소), 컷신은 원본 크기
아래 화면: 평소에는 조작법과 세이브 슬롯 상태,
           대화 중에는 한글 대사/선택지를 원래 크기로 표시 (터치로 선택 가능)

[조작]
 십자키 / 슬라이드패드 : 이동 (위=점프, 아래=숙이기)
 A        : 사용 / 대화 선택 / 대사 넘기기
 B        : 무기 꺼내기 / 발사
 Y 또는 R : 달리기 / 무기 넣기
 X        : 가방
 L        : 상태바
 START    : 메뉴
 ZR / ZL  : 퀵세이브 / 퀵로드 (New 3DS)
 SELECT + L / R : 세이브 슬롯 - / +
 START + SELECT : 종료
 대화 중 터치 : 선택지를 한 번 누르면 선택, 한 번 더 누르면 결정 / 대사 넘기기

[데이터 오류]
데이터 파일을 못 찾으면 아래 화면에 오류 메시지가 뜹니다. START로 종료.
