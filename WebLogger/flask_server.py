from flask import Flask, request, render_template_string
from flask_socketio import SocketIO, emit

app = Flask(__name__)
socketio = SocketIO(app)

logs = []

@app.route('/log', methods=['POST'])
def log():
    raw_log = request.form.get('log')
    if raw_log:
        log_entry = parse_log(raw_log)
        if log_entry['header'] == 'Added':
            logs.append(log_entry)
        # elif log_entry['header'] == 'Removed':
        #     logs[:] = [log for log in logs if log['url'] != log_entry['url']]
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
    }
    for line in lines[1:]:
        if line.startswith('URL:'):
            log_entry['url'] = line.split(': ', 1)[1]
        elif line.startswith('Data:'):
            log_entry['data'] = line.split(': ', 1)[1]
        elif line.startswith('Length:'):
            log_entry['length'] = line.split(': ', 1)[1]
        elif line.startswith('LRU Time Track:'):
            log_entry['lru_time_track'] = line.split(': ', 1)[1]
    return log_entry

@app.route('/')
def index():
    return render_template_string('''
<!DOCTYPE html>
<html>
<head>
    <title>Cache Logs</title>
    <style>
        table {
            width: 100%;
            border-collapse: collapse;
        }
        th, td {
            padding: 8px;
            text-align: left;
            border: 1px solid #ddd;
        }
        th {
            background-color: #f2f2f2;
        }
    </style>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/socket.io/4.0.0/socket.io.js"></script>
    <script type="text/javascript">
        document.addEventListener("DOMContentLoaded", function() {
            var socket = io.connect('http://' + document.domain + ':' + location.port);

            socket.on('update_log', function(log) {
                var table = document.getElementById("logTable");
                var row = table.insertRow(-1);

                var headerCell = row.insertCell(0);
                var urlCell = row.insertCell(1);
                var dataCell = row.insertCell(2);
                var lengthCell = row.insertCell(3);
                var lruTimeTrackCell = row.insertCell(4);

                headerCell.innerHTML = log.header;
                urlCell.innerHTML = log.url;
                dataCell.innerHTML = log.data;
                lengthCell.innerHTML = log.length;
                lruTimeTrackCell.innerHTML = log.lru_time_track;
            });
        });
    </script>
</head>
<body>
    <h1>Cache Logs</h1>
    <table id="logTable">
        <tr>
            <th>Header</th>
            <th>URL</th>
            <th>Data</th>
            <th>Length</th>
            <th>LRU Time Track</th>
        </tr>
        {% for log in logs %}
        <tr>
            <td>{{ log.header }}</td>
            <td>{{ log.url }}</td>
            <td>{{ log.data }}</td>
            <td>{{ log.length }}</td>
            <td>{{ log.lru_time_track }}</td>
        </tr>
        {% endfor %}
    </table>
</body>
</html>
    ''', logs=logs)

if __name__ == '__main__':
    socketio.run(app, debug=True)
