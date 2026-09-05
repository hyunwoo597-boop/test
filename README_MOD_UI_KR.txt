RE5 한글패치 + 캐릭터 모드 설치 프로그램
미리보기 스타일 UI 적용판

[캐릭터]
- 없음 (기본 셰바)
- 엑셀라
- 레베카
- 질 코스튬 1
- 질 코스튬 2

질 코스튬 3/4는 제거됨.

[추가 기능]
- 코스튬 + 추가 스토리 전체 언락 ON/OFF
- 캐릭터 모드와 언락은 서로 main을 덮어쓰지 않도록
  조합별 통합 main을 payload에 별도로 포함함.

예:
- 언락만
- 엑셀라
- 엑셀라 + 전체 언락
- 레베카
- 레베카 + 전체 언락
- 질 코스튬 1
- 질 코스튬 1 + 전체 언락
- 질 코스튬 2
- 질 코스튬 2 + 전체 언락

[조작]
메인 메뉴:
- 위/아래: 이동
- A: 선택
- +: 종료

캐릭터 선택:
- 위/아래: 캐릭터 선택
- A: 현재 캐릭터 + 언락 설정 설치
- B: 뒤로
- X: 현재 선택 조합 설치 상태 확인
- Y: 전체 언락 ON/OFF
- +: 종료

[한글패치 기준]
2026-09-05 사용자가 제공한 최신 한글패치 4개 ARC 반영 완료.
- CoreResource.arc
- GuiTextResource.arc
- Msg2Resource_e.arc
- NXStrapResource.arc

이 프로젝트의 preflight 해시도 위 최신 파일 기준으로 갱신됨.


[최종 빌드 정보]
- 프로젝트 버전: 17.0.0
- 설치기 Title ID: 0100F5A0C0DE0000
- NSP 아이콘: 사용자가 제공한 4인 BIOHAZARD 5 이미지 적용
- 아이콘 SHA-256: 3dc4158938357a8559841b79e0b3631621fb24183d4b3ad41a0d2e87c2003b3c
- 최종 상태: FINAL_BUILD_READY

[빌드 출력]
- RE5_Mod_Installer.nsp
- RE5_Update_1.11_Korean_Mod_AllInOne.nsp
- SHA256SUMS.txt

[필요 환경]
- devkitPro/devkitA64 + libnx
- prod.keys (로컬 파일 또는 GitHub Secret PROD_KEYS_B64)
- RE5_Update_1.11.nsp (input 폴더 또는 GitHub Release Asset)

[로컬 빌드]
PROD_KEYS=/path/to/prod.keys bash ./tools/build_installer_nsp.sh

[통합 NSP 생성]
python3 tools/merge_nsp.py input/RE5_Update_1.11.nsp RE5_Mod_Installer.nsp RE5_Update_1.11_Korean_Mod_AllInOne.nsp
