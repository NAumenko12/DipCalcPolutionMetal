# Kafka / Redpanda В Проекте

## Зачем Здесь Kafka

Расчет поля концентраций может быть тяжелой операцией. Если выполнять его прямо внутри HTTP-запроса, пользователь будет ждать, API будет занят, а при ошибке соединения расчет может потеряться.

Поэтому используется паттерн `async job`:

1. API быстро создает задачу.
2. API отправляет событие в Kafka.
3. Worker забирает событие и считает результат.
4. Frontend периодически спрашивает статус задачи.

## Topic

Основной topic:

```text
concentration.calculate.requested
```

Событие содержит:

- `jobId`
- `referenceLocationId`
- `metalId`
- `year`
- `gridStepKm`
- `areaSizeKm`

## Статусы Job

- `queued`: задача создана и ожидает worker.
- `processing`: worker начал расчет.
- `completed`: расчет завершен, точки сохранены.
- `failed`: worker не смог выполнить расчет.
- `publish_failed`: API создал job, но не смог отправить событие в Kafka.

## Redpanda

Вместо классической Kafka используется Redpanda. Для проекта это удобно:

- совместима с Kafka protocol;
- проще запускается в Docker;
- есть Redpanda Console;
- подходит для локальной разработки и демонстрации.

Console:

```text
http://localhost:8081
```

## Что Можно Улучшить

Для более production-варианта можно добавить:

- retry policy;
- dead-letter topic;
- отдельные события `calculation.completed` и `calculation.failed`;
- идемпотентную обработку повторных сообщений;
- метрики consumer lag;
- structured logs.
