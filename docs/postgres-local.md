# Локальный PostgreSQL

Документ описывает запуск проекта с PostgreSQL, установленным на компьютере.

## 1. Создать Базу

```bash
createdb testdip
```

Если база уже создана, этот шаг можно пропустить.

Примеры строк подключения:

```text
postgresql://localhost:5432/testdip
postgresql://postgres:password@localhost:5432/testdip
postgresql://testdip:testdip@localhost:5432/testdip
```

## 2. Применить Схему

Из корня проекта:

```bash
psql "postgresql://localhost:5432/testdip" -f migrations/001_init.sql
psql "postgresql://localhost:5432/testdip" -f migrations/002_seed_metals.sql
```

Если используется другой пользователь или пароль, нужно заменить строку подключения.

## 3. Проверить Таблицы

```bash
psql "postgresql://localhost:5432/testdip"
```

SQL-проверки:

```sql
SELECT COUNT(*) FROM locations;
SELECT COUNT(*) FROM metals;
SELECT COUNT(*) FROM samples;
SELECT * FROM metals LIMIT 5;
```

## 4. Подключить API К Локальной Базе

```bash
export TESTDIP_DATABASE_URL="postgresql://localhost:5432/testdip"
```

С паролем:

```bash
export TESTDIP_DATABASE_URL="postgresql://postgres:YOUR_PASSWORD@localhost:5432/testdip"
```

C++ API и worker читают строку подключения из переменной `TESTDIP_DATABASE_URL`.

## 5. Загрузка Подготовленного Набора Данных

Для демонстрации можно загрузить подготовленный набор данных через SQL или CSV-импорт. В приложении предусмотрен CSV-импорт проб в разделе `Данные`.

Ожидаемые поля CSV:

```csv
locationId,metalId,value,samplingDate,position,repetition,analyticsNumber
8,16,0.001,2014-07-15,подкроновая,1,manual-import
```

После загрузки данных рекомендуется проверить количество записей:

```sql
SELECT COUNT(*) FROM locations;
SELECT COUNT(*) FROM metals;
SELECT COUNT(*) FROM samples;
```
