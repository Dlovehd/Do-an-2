const express = require('express');
const bodyParser = require('body-parser');
const cors = require('cors');
const mysql = require('mysql2');
const path = require("path")

// Khởi tạo app
const app = express();
const PORT = 3000;

// Middleware
app.use(bodyParser.json()); // Để xử lý JSON
app.use(cors()); // Cho phép yêu cầu từ các domain khác (Cross-Origin Resource Sharing)
app.use(express.static(path.join(__dirname, 'public')));

// Kết nối MySQL
const db = mysql.createConnection({
  host: 'localhost',    // Địa chỉ MySQL server
  user: 'root',         // Tài khoản MySQL
  password: '123456',   // Mật khẩu MySQL
  database: 'attendance_system' // Tên cơ sở dữ liệu
});

// Kết nối MySQL
db.connect(err => {
  if (err) {
    console.error('Lỗi kết nối MySQL:', err);
    process.exit(1); // Dừng server nếu không thể kết nối
  }
  console.log('Đã kết nối MySQL');
});

// API để lấy danh sách học sinh
app.get('/students', (req, res) => {
  const query = 'SELECT * FROM student'; // Query để lấy danh sách học sinh
  db.query(query, (err, results) => {
    if (err) {
      console.error('Lỗi khi lấy dữ liệu:', err);
      res.status(500).send('Lỗi khi lấy dữ liệu');
      return;
    }
    res.json(results); // Trả về dữ liệu dưới dạng JSON
  });
});

app.get('/', (req, res) => {
  // Trả về file index.html trong thư mục root
  res.sendFile(path.join(__dirname, 'index.html'));
});

function isMacAddress(mac) {
  const regex = /^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$/;
  return regex.test(mac);
}

// API để thêm thông tin học sinh mới
app.post('/students', (req, res) => {
  const { mssv, name, mac_address } = req.body; // Lấy dữ liệu từ client

  // Kiểm tra dữ liệu đầu vào
  if (!mssv || !name || !mac_address) {
    return res.status(400).send('Thiếu thông tin cần thiết');
  }

  if (!isMacAddress(mac_address)) {
    return res.status(400).send('Sai kiểu dữ liệu địa chỉ MAC');
  }

  // Câu lệnh INSERT INTO để thêm sinh viên vào bảng
  const query = 'INSERT INTO student (mssv, name, mac_address) VALUES (?, ?, ?)';

  // Thực thi câu lệnh query với dữ liệu đầu vào
  db.query(query, [mssv, name, mac_address], (err, results) => {
    if (err) {
      console.error('Lỗi khi thêm dữ liệu:', err);
      return res.status(500).send('Lỗi khi thêm dữ liệu');
    }

    // Trả về phản hồi thành công nếu thêm thành công
    res.status(201).send('Sinh viên đã được thêm vào cơ sở dữ liệu');
  });
});

// API để xóa học sinh
app.delete('/students/:mssv', (req, res) => {
  const studentMssv = req.params.mssv;

  const query = 'DELETE FROM student WHERE mssv = ?';
  db.query(query, [studentMssv], (err, results) => {
    if (err) {
      console.error('Lỗi khi xóa học sinh:', err);
      res.status(500).send('Lỗi khi xóa học sinh');
      return;
    }
    if (results.affectedRows === 0) {
      return res.status(404).send('Học sinh không tồn tại');
    }
    res.send('Học sinh đã bị xóa');
  });
});

app.get('/students/:mssv', (req, res) => {
  const studentMssv = req.params.mssv;

  const query = 'SELECT * FROM student WHERE mssv = ?';
  db.query(query, [studentMssv], (err, results) => {
    if (err) {
      res.status(400).send('Lỗi khi tìm học sinh');
      return;
    }
    if (results.length === 0) {
      return res.status(404).send('Học sinh không tồn tại');
    }
    res.status(200).send(results[0]);
  });

});


app.get('/access-history', (req, res) => {
  const { date } = req.query;

  if (!date) {
    return res.status(400).json({ error: 'Date parameter is required' });
  }

  // SQL query to select attendance records for the specified date
  const query = 'SELECT * FROM access WHERE DATE(time) = ? ORDER BY time DESC';

  db.query(query, [date], (err, results) => {
    if (err) {
      console.error('Error executing query:', err.message);
      return res.status(500).json({ error: 'Internal server error' });
    }

    res.status(200).json(results);
  });
});


// API để kiểm tra địa chỉ MAC
app.post('/check-mac', (req, res) => {
  const { mac_address } = req.body; // Lấy địa chỉ MAC từ ESP32 gửi lên

  // Kiểm tra dữ liệu đầu vào
  if (!mac_address) {
    return res.status(400).send('Thiếu địa chỉ MAC');
  }

  // Truy vấn cơ sở dữ liệu để kiểm tra MAC
  const query = 'SELECT * FROM student WHERE mac_address = ?';
  db.query(query, [mac_address], (err, results) => {
    if (err) {
      console.error('Lỗi khi truy vấn cơ sở dữ liệu:', err);
      return res.status(500).send('Lỗi server');
    }

    if (results.length === 0) {
      // Nếu không tìm thấy MAC, thêm bản ghi vào bảng access với mssv = null
      const accessQuery = 'INSERT INTO access (mac_address, mssv) VALUES (?, NULL)';
      db.query(accessQuery, [mac_address], (err) => {
        if (err) {
          console.error('Lỗi khi thêm vào bảng access:', err);
        } else {
          const attendanceHistoryQuery = 'SELECT * FROM access WHERE mac_address = ? ORDER BY time DESC LIMIT 1';
          db.query(attendanceHistoryQuery, [mac_address], (_, attendanceResult) => {
            accessHistory.push({ ...attendanceResult[0] });
          });
        }
      });

      // Trả về lỗi 404
      return res.status(404).send('Không tìm thấy sinh viên với địa chỉ MAC này');
    }

    // Nếu tìm thấy MAC, thêm bản ghi vào bảng attendance
    const mssv = results[0].mssv;
    const attendanceQuery = 'INSERT INTO attendance (mssv) VALUES (?)';
    db.query(attendanceQuery, [mssv], (err, result) => {
      if (err) {
        console.error('Lỗi khi thêm điểm danh:', err);
      } else if (result.affectedRows === 1) {
        const attendanceHistoryQuery = 'SELECT * FROM attendance WHERE mssv = ? ORDER BY time DESC LIMIT 1';
        db.query(attendanceHistoryQuery, [mssv], (_, attendanceResult) => {
          attendanceHistory.push({ ...attendanceResult[0], name: results[0].name });
        });
      }
    });

    // Thêm bản ghi vào bảng access với mssv hợp lệ
    const accessQuery = 'INSERT INTO access (mac_address, mssv) VALUES (?, ?)';
    db.query(accessQuery, [mac_address, mssv], (err) => {
      if (err) {
        console.error('Lỗi khi thêm vào bảng access:', err);
      } else {
        const attendanceHistoryQuery = 'SELECT * FROM access WHERE mac_address = ? ORDER BY time DESC LIMIT 1';
        db.query(attendanceHistoryQuery, [mac_address], (_, attendanceResult) => {
          accessHistory.push({ ...attendanceResult[0] });
        });
      }
    });

    // Trả về thông tin sinh viên nếu MAC tồn tại
    res.json(results[0]);
  });
});

app.get('/attendance-history', (req, res) => {
  const { date } = req.query;

  if (!date) {
    return res.status(400).json({ error: 'Date parameter is required' });
  }

  // SQL query to select attendance records for the specified date
  const query = 'SELECT * FROM attendance WHERE DATE(time) = ? ORDER BY time DESC';

  db.query(query, [date], (err, results) => {
    if (err) {
      console.error('Error executing query:', err.message);
      return res.status(500).json({ error: 'Internal server error' });
    }

    res.status(200).json(results);
  });
});

let attendanceHistory = [];

// SSE endpoint
app.get('/attendance-stream', (req, res) => {
  res.setHeader('Content-Type', 'text/event-stream');
  res.setHeader('Cache-Control', 'no-cache');
  res.setHeader('Connection', 'keep-alive');

  const interval = setInterval(() => {
    // Stream new attendance updates
    while (attendanceHistory.length > 0) {
      const update = attendanceHistory.shift(); // Send one update at a time
      res.write(`data: ${JSON.stringify(update)}\n\n`); // Fixed the syntax error
    }
  }, 1000);

  // Clean up when the connection is closed
  req.on('close', () => {
    clearInterval(interval);
    res.end();
  });
});


let accessHistory = [];

// SSE endpoint
app.get('/access-stream', (req, res) => {
  res.setHeader('Content-Type', 'text/event-stream');
  res.setHeader('Cache-Control', 'no-cache');
  res.setHeader('Connection', 'keep-alive');

  const interval = setInterval(() => {
    // Stream new attendance updates
    while (accessHistory.length > 0) {
      const update = accessHistory.shift(); // Send one update at a time
      res.write(`data: ${JSON.stringify(update)}\n\n`); // Fixed the syntax error
    }
  }, 1000);

  // Clean up when the connection is closed
  req.on('close', () => {
    clearInterval(interval);
    res.end();
  });
});

// Khởi chạy server
app.listen(PORT, () => {
  console.log(`Server is running at http://localhost:${PORT}`);
});
