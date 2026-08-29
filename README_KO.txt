RE5 All-In-One GitHub FINAL - Release Asset 방식

이 ZIP에는 75~78MB 업데이트 NSP가 포함되어 있지 않습니다.
따라서 GitHub 웹 업로드 25MB 제한에 걸리지 않습니다.

[1] 저장소에 업로드
- 이 ZIP을 PC에서 압축 해제
- 압축 안의 파일/폴더 전체를 GitHub 저장소 루트에 업로드
- 기존 파일이 있으면 덮어쓰기

[2] GitHub Release 만들기
- 저장소 오른쪽 Releases > Create a new release
- Tag 예: re5-update-1.11
- Release title은 아무거나 가능
- Asset에 업데이트 NSP를 업로드
- 파일명은 반드시 정확히:
  RE5_Update_1.11.nsp
- Publish release

주의:
Actions는 '최신(latest) Release'에서 위 정확한 파일명을 찾습니다.

[3] Secret
- 기존 PROD_KEYS_B64 Secret 유지
- prod.keys 원문은 저장소에 올리지 않음

[4] 빌드
- Actions > Build RE5 All-In-One NSP > Run workflow

[5] 성공 결과 Artifact
RE5-Update-1.11-KoreanPatch-AllInOne-FINAL
안에:
- RE5_Korean_Patch_Installer.nsp
- RE5_Update_1.11_KoreanPatch_AllInOne.nsp
- SHA256SUMS.txt

자동 흐름:
저장소(소형 빌드 소스)
 -> 최신 Release에서 RE5_Update_1.11.nsp 다운로드
 -> ARC 3개/소스/NSP 구조 사전검사
 -> 한글패치 Installer NSP 빌드
 -> Update NSP + Installer NSP를 한 PFS0 멀티타이틀 NSP로 병합
 -> Artifact 업로드

v6 수정:
- devkitPro 컨테이너에 없는 aarch64-none-elf-nm 의존성 제거.
- ELF 링크 성공 자체로 crt0 -> main 해결을 검증하고 ELF/NSO/NPDM 실파일을 확인.
- 이전 실행 로그에서 ELF와 NSO/NSP가 실제 생성된 뒤 검증 명령만 실패한 문제를 수정.

v7 수정:
- hacBrewPack 공식 CLI에 없는 --titleid 옵션 제거.
- hacBrewPack은 control/control.nacp에서 TitleID를 읽음.
- NACP TitleID 0100F5A0C0DE0000 유지.
