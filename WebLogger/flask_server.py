import os
from flask import Flask, request, render_template,jsonify
from flask_socketio import SocketIO, emit
import datetime 

app = Flask(__name__)
socketio = SocketIO(app)

logs = []
IP_FILE_PATH = os.path.join(os.path.dirname(__file__), '..', 'ProxyServer', 'blocked_ips.txt')


@app.route('/log', methods=['POST'])
def log():
    raw_log = request.form.get('log')
    if raw_log:
        log_entry = parse_log(raw_log)
        if log_entry['header'] == 'Added':
            logs.append(log_entry)
        elif log_entry['header'] == 'Removed':
            logs[:] = [log for log in logs if log['url'] != log_entry['url']]
        socketio.emit('update_log', log_entry)
    return 'Log received', 200

def parse_log(raw_log):
    print(raw_log)
    lines = raw_log.split('\n')
    log_entry = {
        'header': lines[0],
        'url': '',
        'data': '',
        'length': '',
        'lru_time_track': '',
        'server': '',
        'date': '',
        'content_type': '',
        'content_length': '',
        'location': '',
        'x_cache': ''
    }
    total_seconds=0
    for line in lines[1:]:
        if line.startswith('URL:'):
            log_entry['url'] = line.split(': ', 1)[1]
        elif line.startswith('Data:'):
            log_entry['data'] = line.split(': ', 1)[1]
        elif line.startswith('Length:'):
            log_entry['length'] = line.split(': ', 1)[1]
        elif line.startswith('Date:'):
            log_entry['date'] = line.split(': ', 1)[1]    
            time_str=log_entry['date']
            time_part = time_str.split(' ')
            time_t=time_part[4]
            hours, minutes, seconds = map(int, time_t.split(':'))
            print("hours:",type(hours))
            print("minutes:",type(minutes))
            print("seconds:",type(seconds))
            total_seconds += hours * 3600 + minutes * 60 + seconds            
           
        elif line.startswith('LRU Time Track:'):
            log_entry['lru_time_track'] = line.split(': ', 1)[1]  
            # time_str = line.split(': ', 1)[1]
            # time_str=int(time_str)
            # response_time=time_str-total_seconds
            # log_entry['lru_time_track'] = response_time
            # try:
            #     time_float = float(time_str)
            #     dt_object = datetime.datetime.fromtimestamp(time_float)
            #     formatted_time = dt_object.strftime("%Y-%m-%d %H:%M:%S")
            #     log_entry['lru_time_track'] = formatted_time
            # except ValueError:
            #     log_entry['lru_time_track'] = 'Error!'
        elif line.startswith('Server:'):
            log_entry['server'] = line.split(': ', 1)[1]
        
        elif line.startswith('Content-Type:'):
            log_entry['content_type'] = line.split(': ', 1)[1]
        elif line.startswith('Content-Length:'):
            log_entry['content_length'] = line.split(': ', 1)[1]
        elif line.startswith('Location:'):
            log_entry['location'] = line.split(': ', 1)[1]
    return log_entry

@app.route('/')
def index():
    return render_template('weblogger.html', logs=logs)

@app.route('/get_logs')
def get_logs():
    return jsonify(logs)

@app.route('/block', methods=['GET', 'POST'])
def block_site():
    if request.method == 'POST':
        ip_address = request.form.get('ip')
        try:
            with open(IP_FILE_PATH, 'a') as f:
                f.write(f"{ip_address}\n")
            socketio.emit('block_ip', {'ip': ip_address})
            return jsonify({"message": f"IP {ip_address} has been blocked successfully!"})
        except Exception as e:
            return jsonify({"error": f"An error occurred while blocking the IP: {e}"})
    return render_template('blockip.html')

if __name__ == '__main__':
    socketio.run(app, debug=True)
