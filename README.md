# sw_battle_test

Пошаговая боевая система с юнитами на клеточной карте. Условие задания: [docs/SPEC.md](docs/SPEC.md),
ограничения архитектуры: [docs/LIMITATIONS.md](docs/LIMITATIONS.md).

## Сборка и запуск

Нужны CMake ≥ 3.27 и компилятор с C++23. Сторонние библиотеки (Boost.Describe, Boost.Mp11, doctest) CMake скачивает
сам, версии закреплены хешем.

```bash
cmake --preset release && cmake --build --preset release
./build/release/Release/sw_battle_test scenario_1.txt --seed 42
```

С одинаковым `--seed` бой повторяется в точности. Без флага seed случайный и печатается в stderr.
Пресеты: `debug`, `release`, `asan` (ASan + UBSan), `coverage`. Инструменты для разработки ставит
`tools/scripts/<mac|linux|win>/setup`.

## Структура

```
src/
├── Core/          движок: события, компоненты, мир, запросы, симуляция, лог, команды сценария
├── Features/      механики юнитов: Common (атаки, марш, полёт, неуязвимость), Swordsman, Hunter, Blueprint
├── App/           программа: аргументы, сборка всего вместе
└── main.cpp
```

Ядро не знает о фичах, фичи не знают друг о друге (проверяет `check.py layers`).

## Как устроен юнит

- **Юнит**: класс с набором компонентов (`SwordsmanKit`), объявленным через `SW_REFLECT`. Порядок полей задаёт приоритет
  действий.
- **Компонент** реагирует на события и вопросы одной функцией `on`:
  - событие (`Turn`, `HitAttempt`, `HitTaken`, `RoundEnd`): `Effects on(const E&)`;
  - вопрос (`Targeted`, `SpeedQuery`, `PlacementQuery`): `Answer on(const Q&) const`.
- **Эффекты**: значения, которые выполняет `Executor`, например
  `return effect::useAbility(a, Name) | effect::attack(a, t, d, Name);`.
- **Компоненты можно добавлять во время боя**: `effect::apply(target, component, source)`, `effect::unapply(target, source)`,
  `effect::expire()`; `Timed<C>` ограничивает срок.
- **Теги**: способность перечисляет виды своего урона (`using Tags = TagList<common::Wound>;`), другие фичи реагируют
  на вид (`taken.is<common::Wound>()`), а не на конкретную способность, поэтому фичи не знают друг о друге.
- **`Dynamic`**: компонент, обработчики которого задаются функциями во время выполнения (поведение без своего типа или
  из скрипта), с теми же сигнатурами.

Чтобы добавить юнита, скопируйте `src/Features/Blueprint/`, добавьте команду `SPAWN_<ИМЯ>` в `Features.cpp` и тесты в
`tests/unit/Features/<Имя>/`.

## Тесты и проверки

```bash
ctest --preset debug                             # юнит-тесты (doctest), e2e-сценарии, реплей фаззеров
uv run tools/scripts/check.py coverage           # 100% строк и ветвлений каждого файла src/
uv run tools/scripts/check.py fuzz battle        # libFuzzer, 60 секунд
tools/scripts/mac/update-expected.sh [имя]       # перезаписать ожидаемый вывод e2e
```

Перед коммитом lefthook запускает `check.py format`, `tidy` (включая проверки проекта `sw-include-style`,
`sw-constrained-templates`, `sw-documented-api`), `layers` и `markdown`, перед push запускается `coverage`. Любое
предупреждение компилятора или clang-tidy считается ошибкой.
