# Effect Controls Panel 연구 노트

## Command IDs

| ID | 이름 | 설명 |
|----|------|------|
| 2163 | EffectControls | EC 패널 토글 (열기/닫기) |
| 3734 | OpenEffectControls | EC 열기만 (토글 아님, 히스토리에 추가) |
| 3700 | Split with New Locked Viewer | 현재 활성 패널을 잠긴 창으로 분리 |

---

## 발견 사항

### EC 히스토리/목록
- EC 패널 상단에 레이어 히스토리가 저장됨
- 목록이 비어있으면 → EC 빈 창
- 목록 있으면 → 해당 레이어 표시
- `3734` 실행 시 선택된 레이어가 목록에 추가됨
- 이펙트 추가해도 목록에 추가됨

### 잠긴 EC 창 생성 문제
- AE는 한 레이어에 대해 두 개의 EC 뷰를 지원하지 않음
- 기존 EC가 레이어를 점유하면 새 잠긴 EC는 빈 상태
- 3700 실행 시 EC 히스토리의 마지막 레이어가 표시됨

### 크래시 조건
- EC 목록이 비어있는 상태 + EC 창 없음 + 잠긴 EC 생성 시도 → AE 크래시 발생
- scheduleTask 중첩 사용 시 불안정

---

## 시도한 방법들

### 1. 단순 연속 실행 (실패)
```javascript
app.executeCommand(2163);  // EC 열기
app.executeCommand(3700);  // 스플릿
```
- 문제: 타이밍 문제로 다른 패널이 스플릿될 수 있음

### 2. scheduleTask 사용 (불안정)
```javascript
app.executeCommand(2163);
app.scheduleTask("app.executeCommand(3700);", 100, false);
```
- 문제: EC가 이미 선택된 상태면 2163이 EC를 닫아버림

### 3. 레이어 전환 후 열기 (불안정)
```javascript
// 임시 레이어로 전환 → EC 닫기 → 원래 레이어 선택 → 새 EC 열기
```
- 문제: AE가 마지막 잠긴 EC 레이어를 기억해서 예상대로 동작 안 함

### 4. 3734 사용 (부분 성공)
```javascript
app.executeCommand(3734);  // EC 열기 (토글 아님)
app.scheduleTask("app.executeCommand(3700);", 100, false);
```
- 결과: EC 히스토리에 추가되지만 잠긴 EC 생성은 불안정

---

## 최종 결론

**잠긴 EC 자동 생성은 AE 한계로 불안정** → 기능 제외

**안전한 방법:**
```javascript
// 선택된 레이어의 EC 열기만
app.executeCommand(3734);
```
- 잠금은 사용자가 수동으로 EC 패널 메뉴에서 설정

---

## 관련 API

### Viewer 객체 (제한적)
```javascript
app.activeViewer           // 현재 활성 뷰어
viewer.type               // VIEWER_COMPOSITION, VIEWER_LAYER, VIEWER_FOOTAGE
viewer.setActive()        // 뷰어 활성화
```
- `VIEWER_EFFECT_CONTROLS` 없음 - EC는 Viewer가 아닌 Panel

### scheduleTask
```javascript
app.scheduleTask(stringToExecute, delay, repeat)
// delay: 밀리초
// repeat: true면 반복 실행
// 반환: taskID
```

### app.effects (이펙트 목록)
```javascript
app.effects[i].displayName  // 로컬라이즈된 이름 (한글 AE면 한글)
app.effects[i].category     // 카테고리
app.effects[i].matchName    // 내부 이름 (적용용)
```

---

## 참고 링크

- [After Effects Command IDs](https://hyperbrew.co/blog/after-effects-command-ids/)
- [AE Scripting Guide](https://ae-scripting.docsforadobe.dev/)
- [Command ID JSON](https://github.com/hyperbrew/after-effects-command-ids/tree/master/json/)
