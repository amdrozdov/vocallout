import os
import json
from redis import Redis

DB_IP = os.getenv('REDIS_HOST', '127.0.0.1')
DB_PORT = int(os.getenv('REDIS_PORT', '6379'))
DB_USER = os.getenv('REDIS_USER', 'default')
DB_PASS = os.getenv('REDIS_PASS', 'test')
SPACE='vocallout_config'

def main():
    db = Redis(host=DB_IP, port=DB_PORT, db=0, username=DB_USER, password=DB_PASS)
    data = {
        "default": [{"host": "0.0.0.0", "port": 9999}]
    }
    db.set(SPACE, json.dumps(data))
    print('done')
    print(db.get(SPACE))

if __name__ == '__main__':
    main()
