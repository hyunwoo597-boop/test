RE5 한글패치 + 캐릭터 모드 설치 프로그램
미리보기 스타일 UI / 캐릭터 모드·언락 분리판

[캐릭터 모드]
- 엑셀라
- 레베카
- 질

기본 셰바 선택 항목은 제거됨.
질 코스튬 2는 제거됨.
기존 질 코스튬 1의 표시 이름은 "질"로 변경됨.

[전체 언락]
- 코스튬 + 추가 스토리 전체 언락은 메인 메뉴의 별도 항목에서 A 버튼으로 설치.
- 캐릭터 선택 화면에는 언락 ON/OFF나 Y 버튼 옵션이 없음.
- mods_unlock/ 조합 payload는 완전히 제거됨.

중요:
캐릭터 모드와 전체 언락은 둘 다 RE5의 atmosphere/contents/010018100CD46000/exefs/main을 사용한다.
따라서 조합 main을 제거한 이번 분리판에서는 두 기능을 동시에 유지할 수 없으며,
나중에 설치한 쪽이 exefs/main을 교체한다.
- 캐릭터 모드 설치 -> 단독 언락 해제
- 전체 언락 설치 -> 현재 캐릭터 모드 해제

[조작]
메인 메뉴:
- 위/아래: 메뉴 이동
- A: 선택/설치
- X: 한글패치 또는 전체 언락 항목에서 설치 상태 확인
- +: 종료

캐릭터 선택:
- 위/아래: 캐릭터 선택
- A: 현재 선택한 캐릭터 모드 설치
- B: 메인 메뉴로 복귀
- X: 현재 선택 캐릭터 설치 상태 확인
- +: 종료

[한글패치 기준]
2026-09-05 사용자가 제공한 최신 한글패치 4개 ARC.
- CoreResource.arc
- GuiTextResource.arc
- Msg2Resource_e.arc
- NXStrapResource.arc

[최종 빌드 정보]
- 프로젝트 버전: 17.2.0
- 설치기 Title ID: 0100F5A0C0DE0000
- 설치 방식: RE5 업데이트 NSP 미포함, 한글패치 + 캐릭터 모드/전체 언락 설치 전용
- NSP 아이콘: 사용자 제공 4인 BIOHAZARD 5 이미지
- 갤러리: 제거됨
- mods_unlock: 제거됨
- 언락 토글: 제거됨
- 캐릭터/언락: 별도 메뉴 액션

[빌드 출력]
- RE5_Korean_Mod_Installer.nsp
- SHA256SUMS.txt

[GitHub Release에 필요한 파일]
- RE5_v17_FULL_ROMFS.zip 하나만 필요
- RE5_Update_1.11.nsp는 사용하지 않음

[필요 환경]
- devkitPro/devkitA64 + libnx
- prod.keys (GitHub Secret PROD_KEYS_B64)

제작자 : 디시인사이드 스위치CFW 갤러리 : 루카

[배경음악]
- iron_and_bone.mp3를 설치기 전용 BGM으로 추가.
- 실행 중 자동 재생되며 곡 끝에서 처음으로 반복 재생.
- Switch audout 규격에 맞춰 48kHz / 스테레오 / 16-bit PCM으로 변환.
- UI 조작과 파일 설치 중에도 재생이 이어지도록 링 버퍼 방식 사용.
- 배경음악 용도로 원본 대비 -8 dB 감쇠 적용.

[앱 아이콘]
- 사용자 지정 BIOHAZARD 5 '모드 앱' 아이콘으로 변경.

[17.2 변경분 업로드 방식]
- 기존 Release의 RE5_v17_FULL_ROMFS.zip은 그대로 유지.
- 기존 RE5_v17_ROMFS_PATCH_17_1.zip은 유지.
- 새로 RE5_v17_ROMFS_PATCH_17_2.zip만 같은 최신 Release에 추가.
- 17.2 패치 ZIP에는 캐릭터 선택 UI 3개만 포함.
- 셰바 UI / 질2 UI / 질2 payload 삭제는 Actions 다운로드 스크립트가 처리.
- Actions는 기존 FULL ROMFS를 푼 뒤 PATCH를 덮어써서 최종 ROMFS를 구성.
