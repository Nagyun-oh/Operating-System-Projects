## 리팩토링 계획 (Refactoring Plan)

### 목표
- 코드 가독성 향상 및 커널 안정성 확보
- 중복 로직 제거 및 함수 단위 기능 분리

###  주요 수정 사항
1. **Assignment 1-2: Process Tracer**
   -  중복된 ftrace wrapper 제거
   -  불필요한 __SYSCALL_DEFINEX 사용 제거
   -  하드코딩된 syscall 번호(336) 상수화
   -  syscall index 의존성 명확화
   -  로그 메시지 통일
   -  make_rw / make_ro 안정성 강화
   -  함수 선언 위치 정리
   -  static / const 정확히 사용
   -  커널 코딩 컨벤션 정렬
2. **Assignment 1-3: Process Tracer**
   - 중복 코드 정리
   - 시스템 콜 매직 넘버 제거
   - 함수 포인터 타입 추상화 (typedef)
   - 후킹 로직 흐름 구조 개선
   - 디버그 로그 정리 및 최소화
   - 전역 통계 구조 초기화 위치 개선
   - 코드 스타일 및 주석 형식 통일
