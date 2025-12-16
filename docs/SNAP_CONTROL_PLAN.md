# Snap Control 모듈 계획서

## 개요
**단축키**: Ctrl+E
**목적**: 이펙트 검색, 적용, 관리를 위한 플로팅 패널

---

## Mode 1: 타임라인/뷰어에서 Ctrl+E

### 동작
1. 선택된 레이어의 **잠긴(고정된) Effect Controls 창** 열기
2. Effect Controls 창 위에 **검색 CEP 패널**이 따라다님
3. 검색으로 이펙트 찾아서 적용

### 구현 필요 사항
- [ ] ExtendScript로 잠긴 Effect Controls 창 열기
- [ ] CEP 패널이 Effect Controls 위치 추적
- [ ] 이펙트 검색 기능 (matchName 기반)
- [ ] 이펙트 적용: `layer.Effects.addProperty(matchName)`

---

## Mode 2: Effect Controls 위에서 Ctrl+E

### 동작
1. **커스텀 이펙트 프리셋 영역**
   - 이펙트 + 값 포함해서 저장
   - 여러 개 저장 가능 (슬롯 1, 2, 3...)
   - 클릭 시 프리셋 적용

2. **현재 레이어 이펙트 리스트**
   - 레이어에 적용된 이펙트 목록 표시
   - 클릭 시: 해당 이펙트만 펼치고 나머지는 접음

### 구현 필요 사항
- [ ] Effect Controls 패널 포커스 감지
- [ ] 잠긴 EC 창의 레이어 가져오기 (selectedLayers 아님)
- [ ] 커스텀 프리셋 저장/로드 (settings.json)
- [ ] 이펙트 값까지 복사하는 ExtendScript
- [ ] 이펙트 펼침/접힘 제어: `property.propertyGroup.selected = true/false`

---

## 공통 기능

### 자동 닫힘
- Effect Controls 창이 닫히면 검색 패널도 자동으로 닫힘
- 윈도우 상태 모니터링 필요

### UI 요구사항
- 검색창이 Effect Controls 창 위치를 따라다님
- 닫기 버튼 [x]
- ESC 키로 닫기

### 검색창 숨김/표시 토글
```
[펼침 상태]
┌─────────────────────────────┬───┐
│  🔍 검색...                  │ x │
└─────────────────────────────┴───┘

[숨김 상태 - 긴 바 형식]
┌─────────────────────────────────┐
│              ▼                  │  ← 클릭하면 펼침
└─────────────────────────────────┘
```
- 숨김: 검색창과 [x] 버튼 숨겨지고 얇은 바만 표시
- 펼침: 검색창과 [x] 버튼 표시

---

## ExtendScript API 참고

### 잠긴 Effect Controls 창 열기
```javascript
// TODO: 잠긴 EC 창 열기 방법 조사 필요
// app.executeCommand() 또는 다른 방법
```

### 이펙트 펼침/접힘
```javascript
// 모든 이펙트 접기
var fx = layer.Effects;
for (var i = 1; i <= fx.numProperties; i++) {
    fx.property(i).selected = false;
}
// 특정 이펙트만 펼치기
fx.property(targetIndex).selected = true;
```

### 이펙트 값 복사
```javascript
// 프리셋으로 저장하려면 각 속성값을 읽어서 JSON으로 저장
var effect = layer.Effects.property(1);
var preset = {
    matchName: effect.matchName,
    properties: []
};
for (var i = 1; i <= effect.numProperties; i++) {
    var prop = effect.property(i);
    if (prop.canSetValue) {
        preset.properties.push({
            name: prop.name,
            value: prop.value
        });
    }
}
```

---

## 현재 구현 상태

### 완료
- [x] Shift+E 트리거 (→ Ctrl+E로 변경 필요)
- [x] Mode 1 검색 UI 기본 틀
- [x] Mode 2 이펙트 리스트 UI 기본 틀
- [x] 닫기 버튼 [x], ESC 키

### 미완료
- [ ] Ctrl+E로 단축키 변경
- [ ] 잠긴 Effect Controls 창 열기
- [ ] CEP 패널이 EC 창 따라다니기
- [ ] 커스텀 프리셋 저장/로드
- [ ] 이펙트 펼침/접힘 제어
- [ ] 이펙트 실제 적용/삭제 연결
- [ ] EC 창 닫히면 자동 닫힘

---

## 참고 링크
- [After Effects Scripting Guide](https://ae-scripting.docsforadobe.dev/)
- [PropertyGroup.addProperty](https://ae-scripting.docsforadobe.dev/property/propertygroup/)
- [PropertyBase.remove](https://ae-scripting.docsforadobe.dev/property/propertybase/)
- [First-Party Effect Match Names](https://ae-scripting.docsforadobe.dev/matchnames/effects/firstparty/)
