# DipCalcPollutionMetal

DipCalcPollutionMetal — web-приложение для учета проб, ведения справочника тяжелых металлов, анализа концентраций и расчета поля загрязнения вокруг источника выбросов.

Система изначально спроектирована как распределенное приложение:

- C++20;
- Drogon REST API;
- PostgreSQL;
- Redpanda/Kafka для очереди расчетов;
- отдельный C++ worker для асинхронных расчетов;
- React + Vite пользовательский интерфейс;
- Docker Compose;
- GitHub Actions CI;
- OpenAPI-спецификация.

## Функциональность

- Отображение площадок пробоотбора на карте.
- Добавление, изменение и удаление площадок.
- Ведение справочника тяжелых металлов.
- Добавление, изменение и удаление проб.
- Фильтрация проб по году и металлу.
- Просмотр статистики концентраций.
- Запуск расчета поля загрязнения.
- Асинхронная обработка расчета через очередь.
- Просмотр истории расчетов.
- Просмотр деталей выбранного расчета.
- Отображение рассчитанной сетки концентраций на карте.
- Экспорт отчета в PDF.
- Экспорт данных в Excel-совместимый файл.
- CSV-импорт проб.

## Архитектура

- frontend отвечает за карту, таблицы, формы, графики и отчеты;
- API принимает HTTP-запросы, валидирует данные и работает с базой;
- PostgreSQL хранит площадки, металлы, пробы, расчеты и точки сетки;
- Redpanda/Kafka передает расчетные задания от API к worker;
- worker выполняет расчет и сохраняет результат.

Подробное описание: [docs/architecture.md](docs/architecture.md).

## Экраны

- `Карта`: источник выбросов, пробные площадки, рассчитанное поле концентраций.
- `Данные`: площадки, пробы, фильтры, CSV-импорт.
- `Расчеты`: выбор опорной площадки, года, металла и параметров сетки.
- `Детали`: история расчетов, статус, параметры, рассчитанные точки.
- `Отчеты`: PDF и Excel-экспорт.
- `Справочники`: металлы, локации, параметры расчета.
- `Статистика`: график и таблица агрегированных концентраций.

Скриншоты интерфейса и описание экранов находятся в [docs/user-interface.md](docs/user-interface.md).

## API

Основные endpoint:

- `GET /health`
- `GET /api/locations`
- `POST /api/locations`
- `PUT /api/locations/{id}`
- `DELETE /api/locations/{id}`
- `GET /api/locations/{id}/samples`
- `POST /api/locations/{id}/samples`
- `PUT /api/samples/{id}`
- `DELETE /api/samples/{id}`
- `GET /api/metals`
- `POST /api/metals`
- `PUT /api/metals/{id}`
- `DELETE /api/metals/{id}`
- `GET /api/statistics/samples`
- `POST /api/calculations/concentration`
- `GET /api/calculations`
- `GET /api/calculations/{id}`
- `GET /api/calculations/{id}/points`

Полная спецификация находится в [openapi.yaml](openapi.yaml).

## Быстрый Запуск Через Docker

```bash
cd /Users/eugenenaumenko/Desktop/DipCalcPolutionMetal
docker-compose up --build
```

После запуска:

- приложение: http://localhost:8080
- Redpanda Console: http://localhost:8081
- PostgreSQL: `localhost:5432`

Если используется новая команда Docker Compose:

```bash
docker compose up --build
```

## Запуск С Локальным PostgreSQL

Сборка:

```bash
cd /Users/eugenenaumenko/Desktop/DipCalcPolutionMetal

npm --prefix frontend install
npm --prefix frontend run build

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

API:

```bash
TESTDIP_DATABASE_URL=postgresql://localhost:5432/testdip \
TESTDIP_HTTP_PORT=8080 \
TESTDIP_KAFKA_BROKERS=127.0.0.1:9092 \
./build/testdip-api
```

Worker в отдельном терминале:

```bash
TESTDIP_DATABASE_URL=postgresql://localhost:5432/testdip \
TESTDIP_KAFKA_BROKERS=127.0.0.1:9092 \
./build/testdip-worker
```

Важно: для локального запуска с Redpanda лучше использовать `127.0.0.1:9092`, а не `localhost:9092`.

## Проверка

Одна команда:

```bash
./scripts/verify.sh
```

Она выполняет:

- установку frontend-зависимостей;
- сборку frontend;
- конфигурацию CMake;
- сборку C++;
- запуск unit-тестов.

## Поток Расчета

```text
POST /api/calculations/concentration
        |
        v
concentration_jobs: queued
        |
        v
событие concentration.calculate.requested
        |
        v
worker
        |
        v
concentration_jobs: processing
        |
        v
calculation_grid_points
        |
        v
concentration_jobs: completed
```

Подробнее: [docs/kafka.md](docs/kafka.md).

## База Данных

Основные таблицы:

- `locations`: площадки пробоотбора;
- `metals`: справочник тяжелых металлов;
- `samples`: пробы и значения концентраций;
- `concentration_jobs`: история расчетов;
- `concentration_grid_points`: точки рассчитанной сетки.

Подробнее: [docs/data-model.md](docs/data-model.md).

## CI/CD

Workflow находится в [.github/workflows/ci.yml](.github/workflows/ci.yml).

CI проверяет:

- сборку frontend;
- сборку C++;
- unit-тесты;
- Docker build для API;
- Docker build для worker.

Подробнее: [docs/ci-cd.md](docs/ci-cd.md).

## Возможные Улучшения

- Серверная генерация PDF/Excel.
- E2E-тесты пользовательского сценария расчета.
- Авторизация и роли пользователей.
- Расширенное логирование с request id.
- Retry и dead-letter topic для расчетных событий.
- Устранение предупреждений компилятора от текущей версии libpqxx.
