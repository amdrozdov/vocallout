import os
import json
from redis import Redis

REDIS_IP = os.getenv('REDIS_HOST', '127.0.0.1')
REDIS_PORT = int(os.getenv('REDIS_PORT', '6379'))
REDIS_USER = os.getenv('REDIS_USER', 'default')
REDIS_PASS = os.getenv('REDIS_PASS', 'test')
CONFIG_FIELD='vocallout_config'

def main():
    db = Redis(host=REDIS_IP, port=REDIS_PORT, db=0, username=REDIS_USER, password=REDIS_PASS)
    data = {
        "default": [{"host": "0.0.0.0", "port": 9999}]
    }
    db.set(CONFIG_FIELD, json.dumps(data))
    print('done')
    print(db.get(CONFIG_FIELD))

if __name__ == '__main__':
    main()
