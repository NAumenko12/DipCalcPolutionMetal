# CI/CD

## Что Проверяет CI

GitHub Actions workflow находится здесь:

```text
.github/workflows/ci.yml
```

Он выполняет:

- установку Node.js;
- установку C++ зависимостей;
- `npm ci`;
- `npm run build`;
- `cmake`;
- `cmake --build`;
- `ctest`;
- Docker build для API;
- Docker build для worker.

## Локальная Проверка

Перед commit можно выполнить:

```bash
./scripts/verify.sh
```

Скрипт делает то же самое, что основная часть CI:

```text
frontend install -> frontend build -> C++ build -> tests
```

## Что Такое CI/CD В Этом Проекте

CI проверяет, что проект собирается и тесты проходят после каждого push или pull request.

CD можно расширить позже:

- собирать Docker images;
- публиковать images в registry;
- деплоить API и worker на сервер;
- применять изменения схемы базы данных;
- запускать smoke-тесты после деплоя.

Сейчас в проекте реализована CI-часть и Docker build. Этого достаточно для демонстрации инженерного процесса в дипломном проекте.
