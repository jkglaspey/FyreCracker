import socketio
import json

sio = socketio.Client()
sio.connect('http://localhost:3000')
sio.emit('train', json.dumps({ 'model_path': 'model.zip' }))

rdy = False

@sio.on('actions')
def actions(*_):
    global rdy
    rdy = True

for i in range(50):
    data = json.dumps({'observations': {}, 'rewards': {}, 'terminations': {}, 'truncations': {}, 'infos': {}})
    print(f'{i} post {data}')
    sio.emit('env_update', data)
    sio.sleep(0.1)
    while True:
        if rdy:
            rdy = False
            break
        else:
            sio.sleep(0.5)

sio.disconnect()