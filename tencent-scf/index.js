const http = require('http');
const fs = require('fs');
const url = require('url');

const DATA_FILE = '/tmp/verify_data.json';

function loadData() {
    try {
        if (fs.existsSync(DATA_FILE)) {
            return JSON.parse(fs.readFileSync(DATA_FILE, 'utf8'));
        }
    } catch (e) { }
    return {};
}

function saveData(data) {
    try {
        fs.writeFileSync(DATA_FILE, JSON.stringify(data));
    } catch (e) { }
}

function sendJSON(res, code, obj) {
    res.writeHead(code, {
        'Content-Type': 'application/json',
        'Access-Control-Allow-Origin': '*',
        'Access-Control-Allow-Headers': 'Content-Type'
    });
    res.end(JSON.stringify(obj));
}

function sendHTML(res, html) {
    res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
    res.end(html);
}

function parseBody(req) {
    return new Promise((resolve) => {
        let body = '';
        req.on('data', chunk => { body += chunk; });
        req.on('end', () => {
            try { resolve(JSON.parse(body)); }
            catch (e) { resolve({}); }
        });
    });
}

// ======================== 短信密钥配置 ========================
const SMS_CONFIG = JSON.parse(fs.readFileSync(__dirname + '/sms_config.json', 'utf8'));
const SMS_PASSWORD = SMS_CONFIG.password;
const SMS_AUTH_TOKEN = SMS_CONFIG.auth_token;
// ==============================================================

const HTML_PAGE = `<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>快递柜验证</title>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    background: #f0f2f5;
    min-height: 100vh;
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 20px;
}
.card {
    background: #fff;
    border-radius: 16px;
    padding: 32px 24px;
    width: 100%;
    max-width: 400px;
    box-shadow: 0 2px 12px rgba(0,0,0,0.08);
}
.title {
    text-align: center;
    font-size: 22px;
    font-weight: 700;
    color: #1a1a1a;
    margin-bottom: 24px;
}
.form-group { margin-bottom: 16px; }
.form-group label {
    display: block;
    font-size: 14px;
    font-weight: 600;
    color: #333;
    margin-bottom: 6px;
}
.input-row {
    display: flex;
    gap: 8px;
}
.input-row input { flex: 1; }
input {
    width: 100%;
    height: 48px;
    border: 1.5px solid #ddd;
    border-radius: 10px;
    padding: 0 14px;
    font-size: 16px;
    outline: none;
    transition: border-color 0.2s;
}
input:focus { border-color: #1677ff; }
input:disabled { background: #f5f5f5; color: #999; }
.btn {
    height: 48px;
    border: none;
    border-radius: 10px;
    font-size: 16px;
    font-weight: 600;
    cursor: pointer;
    transition: opacity 0.2s;
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 6px;
}
.btn:disabled { opacity: 0.5; cursor: not-allowed; }
.btn-secondary {
    background: #f0f2f5;
    color: #333;
    white-space: nowrap;
    padding: 0 16px;
}
.btn-primary {
    width: 100%;
    background: #1677ff;
    color: #fff;
    margin-top: 8px;
}
.status {
    text-align: center;
    padding: 12px;
    border-radius: 10px;
    font-size: 14px;
    margin-bottom: 8px;
}
.status-error { background: #fff2f0; color: #ff4d4f; }
.status-success { background: #f6ffed; color: #52c41a; }
.result {
    text-align: center;
    padding: 32px 0;
}
.result-icon {
    width: 64px;
    height: 64px;
    border-radius: 50%;
    display: flex;
    align-items: center;
    justify-content: center;
    margin: 0 auto 16px;
    font-size: 32px;
}
.result-icon.success { background: #f6ffed; }
.result-icon.error { background: #fff2f0; }
.result h2 { font-size: 20px; margin-bottom: 8px; }
.result p { color: #666; font-size: 14px; }
.spinner {
    width: 20px;
    height: 20px;
    border: 2px solid #ddd;
    border-top-color: #1677ff;
    border-radius: 50%;
    animation: spin 0.8s linear infinite;
}
@keyframes spin { to { transform: rotate(360deg); } }
</style>
</head>
<body>
<div class="card" id="app"></div>

<script>
const ADMIN_PHONE = "13011110000";
const ADMIN_CODE = "1234";
const PHONE_REGEX = /^1[3-9]\\d{9}$/;

const params = new URLSearchParams(location.search);
const token = params.get("token") || "";

function render() {
    app.innerHTML = token
        ? '<div class="form-group"><label>手机号</label><input id="phone" type="tel" maxlength="11" placeholder="请输入手机号" autofocus></div><div class="form-group"><label>验证码</label><div class="input-row"><input id="code" type="text" maxlength="4" placeholder="4位验证码"><button class="btn btn-secondary" id="sendBtn">获取验证码</button></div></div><div id="statusMsg"></div><button class="btn btn-primary" id="verifyBtn">验证并取件</button>'
        : '<div class="result"><div class="result-icon error">⚠</div><h2>无效链接</h2><p>缺少取件凭证，请联系管理员</p></div>';

    if (!token) return;

    const phone = document.getElementById("phone");
    const code = document.getElementById("code");
    const sendBtn = document.getElementById("sendBtn");
    const verifyBtn = document.getElementById("verifyBtn");
    const statusMsg = document.getElementById("statusMsg");

    let countdown = 0;
    let timer = null;

    function showStatus(text, type) {
        statusMsg.innerHTML = '<div class="status status-' + type + '">' + text + '</div>';
    }

    sendBtn.onclick = function() {
        if (!PHONE_REGEX.test(phone.value) || countdown > 0) return;
        showStatus("验证码已发送（测试模式：请输入1234）", "success");
        code.focus();
        countdown = 60;
        sendBtn.disabled = true;
        timer = setInterval(function() {
            countdown--;
            sendBtn.textContent = countdown + "s";
            if (countdown <= 0) {
                clearInterval(timer);
                sendBtn.textContent = "重新发送";
                sendBtn.disabled = false;
            }
        }, 1000);
    };

    verifyBtn.onclick = async function() {
        if (code.value.length !== 4) return;

        verifyBtn.disabled = true;
        verifyBtn.innerHTML = '<span class="spinner"></span>验证中...';

        try {
            const resp = await fetch("/api/verify", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ token: token, phone: phone.value })
            });
            const data = await resp.json();
            if (data.success) {
                app.innerHTML = '<div class="result"><div class="result-icon success">✅</div><h2>验证成功</h2><p>箱门即将打开，请取走包裹</p></div>';
            } else {
                showStatus(data.error || "验证失败", "error");
                verifyBtn.disabled = false;
                verifyBtn.innerHTML = '验证并取件';
            }
        } catch(e) {
            showStatus("网络错误，请重试", "error");
            verifyBtn.disabled = false;
            verifyBtn.innerHTML = '验证并取件';
        }
    };
}

render();
</script>
</body>
</html>`;

const server = http.createServer(async (req, res) => {
    const parsedUrl = url.parse(req.url, true);
    const pathname = parsedUrl.pathname;

    if (req.method === 'OPTIONS') {
        sendJSON(res, 200, { ok: true });
        return;
    }

    if (pathname === '/api/verify' && req.method === 'POST') {
        const body = await parseBody(req);
        const { token, phone } = body;

        if (!token || !phone) {
            sendJSON(res, 400, { error: '缺少必要参数' });
            return;
        }

        const data = loadData();
        if (data[token]) {
            sendJSON(res, 409, { error: '已验证过', alreadyVerified: true });
            return;
        }

        data[token] = { phone, status: 'verified', createdAt: new Date().toISOString() };
        saveData(data);

        console.log('[验证成功] Token:', token, 'Phone:', phone);
        sendJSON(res, 200, { success: true, message: '验证成功' });
        return;
    }

    if (pathname === '/sms_key' && req.method === 'POST') {
        const body = await parseBody(req);
        const { token } = body;

        if (!token || token !== SMS_AUTH_TOKEN) {
            sendJSON(res, 403, { ok: false, msg: 'auth failed' });
            return;
        }

        console.log('[SMS_KEY] token 验证通过, 返回密码');
        sendJSON(res, 200, { ok: true, password: SMS_PASSWORD });
        return;
    }

    if (pathname === '/api/status' && req.method === 'GET') {
        const token = parsedUrl.query.token;

        if (!token) {
            sendJSON(res, 400, { error: '缺少token参数' });
            return;
        }

        const data = loadData();
        const record = data[token];

        if (record && record.status === 'verified') {
            sendJSON(res, 200, {
                verified: true,
                phone: record.phone,
                verifiedAt: record.createdAt
            });
        } else {
            sendJSON(res, 200, { verified: false });
        }
        return;
    }

    sendHTML(res, HTML_PAGE);
});

server.listen(3000, '0.0.0.0', () => {
    console.log('快递柜验证服务已启动: http://0.0.0.0:3000');
});