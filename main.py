from fastapi import FastAPI, HTTPException
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel
from pathlib import Path
from network import NetworkWrapper
import psycopg2
from psycopg2.extras import RealDictCursor
import os

app = FastAPI()

net = NetworkWrapper()


# Бдшка
DB_CONFIG = {
    "dbname": os.getenv("DB_NAME", "network_sim"),
    "user": os.getenv("DB_USER", "postgres"),
    "password": os.getenv("DB_PASSWORD", ""), # Пароль пустой по умолчанию
    "host": os.getenv("DB_HOST", "localhost"),
    "port": os.getenv("DB_PORT", 5432),
    "client_encoding": "UTF8"
}

def get_db_connection():
    return psycopg2.connect(**DB_CONFIG)
# Очистка для сброса
@app.post("/api/clear-history")
def clear_history():
    conn = get_db_connection()
    cur = conn.cursor()
    cur.execute("TRUNCATE TABLE load_history RESTART IDENTITY")
    conn.commit()
    cur.close()
    conn.close()
    return {"status": "ok"}

# Модели
class AddNodeRequest(BaseModel):
    type: int          # 0=Core, 1=Distribution, 2=Access, 3=Subscriber
    ip: str
    capacity: int
    parent_ip: str = ""

# API
@app.get("/api/state")
def get_state():
    return net.get_state()

@app.post("/api/tick")
def do_tick():
    net.tick()
    state = net.get_state()
    
    # Сохранение состояния в PostgreSQL
    try:
        conn = get_db_connection()
        cur = conn.cursor()
        
        for node in state["nodes"]:
            cur.execute(
                """INSERT INTO load_history 
                   (node_ip, node_type, current_load, sim_hour, sim_day) 
                   VALUES (%s, %s, %s, %s, %s)""",
                (node["ip"], node["type"], node["current_load"], 
                 state["hour"], state["day"])
            )
        
        conn.commit()
        cur.close()
        conn.close()
    except Exception as e:
        print(f"Ошибка записи в БД: {e}")
        
    return state

@app.get("/api/history/{ip}")
def get_history(ip: str, limit: int = 24):
    try:
        conn = get_db_connection()
        cur = conn.cursor(cursor_factory=RealDictCursor)
        
        cur.execute(
            """SELECT sim_hour, sim_day, current_load, created_at 
               FROM load_history 
               WHERE node_ip = %s 
               ORDER BY id DESC LIMIT %s""",
            (ip, limit)
        )
        
        rows = cur.fetchall()
        cur.close()
        conn.close()
        
        # Разворачиваем, чтобы график шёл по возрастанию времени
        return {"ip": ip, "history": list(reversed(rows))}
    except Exception as e:
        raise HTTPException(500, f"Ошибка БД: {e}")
    
@app.post("/api/reset")
def reset():
    global net
    net = NetworkWrapper()
    return net.get_state()

ERROR_MESSAGES = {
    -1: "Сеть не инициализирована",
    -3: "Неверный формат IP",
    -4: "IP уже занят",
    -5: "Не указан родительский узел",
    -6: "Родительский узел не найден",
    -7: "Неверная иерархия: родитель должен быть на уровень выше",
    -8: "Ядро уже существует в сети",
}

@app.post("/api/node")
def add_node(req: AddNodeRequest):
    result = net.add_node(req.type, req.ip, req.capacity, req.parent_ip)
    if result != 0:
        msg = ERROR_MESSAGES.get(result, f"Ошибка добавления узла (код {result})")
        raise HTTPException(400, msg)
    net.save()
    return net.get_state()

@app.delete("/api/node/{ip}")
def remove_node(ip: str):
    result = net.remove_node(ip)
    if result == -2:
        raise HTTPException(400, "Can't delete core")
    if result != 0:
        raise HTTPException(400, f"Failed to remove node (code {result})")
    net.save()
    return net.get_state()

# Фронт
frontend_path = Path(__file__).parent / "frontend"
app.mount("/", StaticFiles(directory=frontend_path, html=True), name="frontend")