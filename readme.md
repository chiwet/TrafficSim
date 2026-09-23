## Установка и запуск

1. **База данных:** Установите PostgreSQL. Создайте базу данных `network_sim`.
2. **Настройка:** Скопируйте файл `.env.example` в `.env` и укажите свой пароль от PostgreSQL.
3. **Схема БД:** Выполните скрипт `db_setup.sql` в вашей базе данных.
4. **Зависимости Python:** `pip install -r requirements.txt`
5. **DLL:** Убедитесь, что файл `Dll1.dll` лежит в папке `x64/Debug/`.
6. **Запуск:** `uvicorn main:app --reload`