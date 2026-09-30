const http = require('http');
const fs = require('fs');
const path = require('path');
const url = require('url');
const crypto = require('crypto');

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
        let size = 0;
        req.on('data', chunk => {
            size += chunk.length;
            if (size > 10 * 1024) {  // 请求体上限 10KB，防内存耗尽
                req.destroy();
                resolve({});
                return;
            }
            body += chunk;
        });
        req.on('end', () => {
            try { resolve(JSON.parse(body)); }
            catch (e) { resolve({}); }
        });
    });
}

// /sms_key 频率限制：按来源 IP 节流，防止短信密码被高频抓取
const SMS_KEY_RATE_LIMIT = 5;      // 窗口内最多次数
const SMS_KEY_RATE_WINDOW = 60 * 1000;  // 60 秒窗口
const smsKeyHits = new Map();

function smsKeyThrottled(ip) {
    const now = Date.now();
    const arr = (smsKeyHits.get(ip) || []).filter(t => now - t < SMS_KEY_RATE_WINDOW);
    if (arr.length >= SMS_KEY_RATE_LIMIT) {
        smsKeyHits.set(ip, arr);
        return true;
    }
    arr.push(now);
    smsKeyHits.set(ip, arr);
    if (smsKeyHits.size > 1000) smsKeyHits.clear();  // 防 Map 无限增长
    return false;
}

// 网页取件发码冷却：按手机号维度（跨 token），防止换 token 对同一号码轰炸
const phoneCooldowns = new Map();  // phone -> lastSendTime

function phoneCooldownCheck(phone) {
    const now = Date.now();
    const last = phoneCooldowns.get(phone) || 0;
    if (now - last < SMS_COOLDOWN_SEC * 1000) {
        return true;  // 冷却中
    }
    phoneCooldowns.set(phone, now);
    if (phoneCooldowns.size > 1000) phoneCooldowns.clear();
    return false;
}

// HTML 转义，防止剩余包裹等数据注入 innerHTML（XSS）
function escHtml(s) {
    return String(s).replace(/[&<>"']/g, function (c) {
        return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
    });
}

// ======================== 短信密钥配置 ========================
const SMS_CONFIG = JSON.parse(fs.readFileSync(__dirname + '/sms_config.json', 'utf8'));
const SMS_PASSWORD = SMS_CONFIG.password;
const SMS_AUTH_TOKEN = SMS_CONFIG.auth_token;
// ==============================================================

// ======================== 网页取件短信验证码 ========================
// 网页端取件验证流程：验证码由服务端生成、发送并校验，
// 杜绝"任意手机号+任意验证码"绕过验证（原 /api/verify 无任何校验）。
const SMS_ACCOUNT = 'C81028909';
const SMS_CODE_EXPIRE_SEC = 300;   // 验证码有效期 5 分钟
const SMS_COOLDOWN_SEC = 60;     // 同一令牌重发冷却
const SMS_MAX_ATTEMPTS = 5;      // 验证码错误次数上限，超过需重新获取

// 测试手机号：与设备端 Demo 账号一致（13011111111 / 1111），
// 发码时免真实短信（固定验证码 1111），仅用于联调；
// 其余号码仍走"服务端生成+真实短信+严格校验"，方案不变
const TEST_PHONE = '13011111111';
const TEST_CODE = '1111';

function hashCode(code) {
    return crypto.createHash('sha256').update(String(code)).digest('hex');
}

/* 调用互亿无线短信接口发送验证码（与设备端 sms.c 同一账号/接口） */
function sendSms(phone, code) {
    return new Promise((resolve) => {
        const body = 'account=' + SMS_ACCOUNT +
            '&password=' + encodeURIComponent(SMS_PASSWORD) +
            '&mobile=' + phone +
            '&content=' + encodeURIComponent('您的验证码是：' + code + '。请不要把验证码泄露给其他人。');
        const req = http.request({
            host: '118.31.68.22',
            port: 80,
            path: '/sms/Submit.json',
            method: 'POST',
            headers: {
                'Host': 'api.ihuyi.com',
                'Content-Type': 'application/x-www-form-urlencoded',
                'Content-Length': Buffer.byteLength(body)
            },
            timeout: 5000
        }, (res) => {
            let data = '';
            res.on('data', (c) => { data += c; });
            res.on('end', () => {
                try {
                    const j = JSON.parse(data);
                    console.log('[短信发送] Phone:', phone, 'code:', j.code, j.msg || '');
                    resolve(j && j.code === 2);
                } catch (e) { resolve(false); }
            });
        });
        req.on('error', () => resolve(false));
        req.on('timeout', () => { req.destroy(); resolve(false); });
        req.write(body);
        req.end();
    });
}
// ==============================================================

// ======================== 快递员账号配置 ========================
// 快递员名单存于云端（/opt/mail-box-api/courier_data.json），嵌入式端
// 登录时通过 GET /api/courier/check 校验手机号是否在名单中。
const COURIER_FILE = '/opt/mail-box-api/courier_data.json';
// 管理 API（add/remove/list）所需密钥，通过 x-api-key 头传入；
// 优先读环境变量，未设置时回退默认值
const COURIER_ADMIN_TOKEN = process.env.COURIER_ADMIN_TOKEN || 'courier_admin_2026';
// 首次运行（名单文件不存在）时的默认快递员
const DEFAULT_COURIERS = ['13012345678', '13011111111'];

function loadCouriers() {
    try {
        if (fs.existsSync(COURIER_FILE)) {
            const data = JSON.parse(fs.readFileSync(COURIER_FILE, 'utf8'));
            if (Array.isArray(data.phones)) return data.phones;
        }
    } catch (e) { }
    return DEFAULT_COURIERS.slice();
}

function saveCouriers(phones) {
    try {
        fs.mkdirSync(path.dirname(COURIER_FILE), { recursive: true });
        fs.writeFileSync(COURIER_FILE, JSON.stringify({ phones }));
    } catch (e) { }
}

function isCourierAdmin(req) {
    return req.headers['x-api-key'] === COURIER_ADMIN_TOKEN;
}
// ==============================================================

// ======================== 用户账号配置 ========================
// 用户数据存于云端（/opt/mail-box-api/user_data.json），结构：
//   { "phones": [...], "records": [...] }
// - phones:  用户名单（/api/user/check 等管理接口用）
// - records: 登录记录（嵌入式端登录成功后异步上报，见 POST /api/login/record）
const USER_FILE = '/opt/mail-box-api/user_data.json';
// 管理 API（add/remove/list）所需密钥，通过 x-api-key 头传入；
// 优先读环境变量，未设置时回退默认值
const USER_ADMIN_TOKEN = process.env.USER_ADMIN_TOKEN || 'user_admin_2026';
// 首次运行（名单文件不存在）时的默认用户
const DEFAULT_USERS = ['13011111111'];

function loadUserData() {
    try {
        if (fs.existsSync(USER_FILE)) {
            const data = JSON.parse(fs.readFileSync(USER_FILE, 'utf8'));
            if (data && typeof data === 'object') return data;
        }
    } catch (e) { }
    /* 文件不存在/损坏：返回默认名单，保证首次写入时 phones 不为空 */
    return { phones: DEFAULT_USERS.slice(), records: [] };
}

function saveUserData(data) {
    try {
        fs.mkdirSync(path.dirname(USER_FILE), { recursive: true });
        fs.writeFileSync(USER_FILE, JSON.stringify(data));
    } catch (e) { }
}

function loadUsers() {
    const data = loadUserData();
    return Array.isArray(data.phones) ? data.phones : DEFAULT_USERS.slice();
}

function saveUsers(phones) {
    const data = loadUserData();
    data.phones = phones;
    saveUserData(data);
}

function isUserAdmin(req) {
    return req.headers['x-api-key'] === USER_ADMIN_TOKEN;
}
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

    sendBtn.onclick = async function() {
        if (!PHONE_REGEX.test(phone.value) || countdown > 0) {
            if (countdown <= 0) showStatus("请输入正确的手机号", "error");
            return;
        }
        sendBtn.disabled = true;
        sendBtn.textContent = "发送中...";
        try {
            const resp = await fetch("/api/send-code", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ token: token, phone: phone.value })
            });
            const data = await resp.json();
            if (resp.ok && data.success) {
                showStatus("验证码已发送到 " + phone.value + "，请查收", "success");
                code.focus();
                countdown = 60;
                sendBtn.textContent = countdown + "s";
                timer = setInterval(function() {
                    countdown--;
                    sendBtn.textContent = countdown + "s";
                    if (countdown <= 0) {
                        clearInterval(timer);
                        sendBtn.textContent = "重新发送";
                        sendBtn.disabled = false;
                    }
                }, 1000);
            } else {
                showStatus(data.error || "发送失败，请重试", "error");
                sendBtn.textContent = "获取验证码";
                sendBtn.disabled = false;
            }
        } catch(e) {
            showStatus("网络错误，请重试", "error");
            sendBtn.textContent = "获取验证码";
            sendBtn.disabled = false;
        }
    };

    verifyBtn.onclick = async function() {
        if (code.value.length !== 4) {
            showStatus("请输入4位验证码", "error");
            return;
        }
        if (!PHONE_REGEX.test(phone.value)) {
            showStatus("请输入正确的手机号", "error");
            return;
        }

        verifyBtn.disabled = true;
        verifyBtn.innerHTML = '<span class="spinner"></span>验证中...';

        try {
            const resp = await fetch("/api/verify", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ token: token, phone: phone.value, code: code.value })
            });
            const data = await resp.json();
            if (data.success) {
                app.innerHTML = '<div class="result"><div class="result-icon success">✅</div><h2>验证成功</h2><p>箱门即将打开，请取走包裹</p><div id="remainingBox"></div></div>';
                pollRemaining();
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

async function pollRemaining() {
    const box = document.getElementById('remainingBox');
    if (!box) return;

    const check = async () => {
        try {
            const resp = await fetch('/api/status?token=' + token);
            const data = await resp.json();
            if (data.remaining && data.remaining.length > 0) {
                let html = '<div style="margin-top:20px;text-align:left"><h3 style="font-size:16px;color:#333;margin-bottom:12px">📦 剩余未取包裹</h3>';
                data.remaining.forEach(function(pkg) {
                    html += '<div style="background:#f6f8fa;border-radius:8px;padding:10px 14px;margin-bottom:8px;display:flex;justify-content:space-between;align-items:center"><span style="font-weight:600;color:#1a1a1a">柜号 ' + escHtml(pkg.lockerId) + '</span><span style="color:#666;font-size:14px">取件码 <b style="color:#1677ff;font-size:16px">' + escHtml(pkg.code) + '</b></span></div>';
                });
                html += '</div>';
                box.innerHTML = html;
                if (data.opened) return;
            }
            setTimeout(check, 2000);
        } catch(e) {
            setTimeout(check, 3000);
        }
    };
    setTimeout(check, 1000);
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

    // ======================== 网页取件：发送验证码 ========================
    // 服务端生成随机验证码 → 互亿无线发送短信 → 哈希存储绑定 (token,phone)
    if (pathname === '/api/send-code' && req.method === 'POST') {
        const body = await parseBody(req);
        const { token, phone } = body;

        if (!token || !phone) {
            sendJSON(res, 400, { error: '缺少必要参数' });
            return;
        }
        if (!/^1[3-9]\d{9}$/.test(phone)) {
            sendJSON(res, 400, { error: '手机号格式错误' });
            return;
        }

        const data = loadData();
        const record = data[token];
        if (record && (record.status === 'verified' || record.status === 'opened')) {
            sendJSON(res, 409, { error: '该令牌已验证' });
            return;
        }

        const now = Date.now();
        if (record && record.smsSentAt && (now - record.smsSentAt) < SMS_COOLDOWN_SEC * 1000) {
            sendJSON(res, 429, { error: '请求过于频繁，请' + Math.ceil((SMS_COOLDOWN_SEC * 1000 - (now - record.smsSentAt)) / 1000) + '秒后再试' });
            return;
        }
        /* 按手机号维度冷却：防止攻击者换 token 对同一号码无限发码 */
        if (phoneCooldownCheck(phone)) {
            sendJSON(res, 429, { error: '该手机号发送过于频繁，请稍后再试' });
            return;
        }

        const isTest = (phone === TEST_PHONE);
        const code = isTest ? TEST_CODE : String(Math.floor(1000 + Math.random() * 9000));
        if (!isTest) {
            const ok = await sendSms(phone, code);
            if (!ok) {
                sendJSON(res, 502, { error: '验证码发送失败，请重试' });
                return;
            }
        } else {
            console.log('[发送验证码] 测试手机号免短信，固定验证码:', TEST_CODE);
        }

        const pending = record || {};
        pending.phone = phone;
        pending.status = 'pending';
        pending.smsCodeHash = hashCode(code);
        pending.smsExpire = now + SMS_CODE_EXPIRE_SEC * 1000;
        pending.smsSentAt = now;
        pending.smsAttempts = 0;
        data[token] = pending;
        saveData(data);

        console.log('[发送验证码] Token:', token, 'Phone:', phone);
        sendJSON(res, 200, { success: true, message: '验证码已发送' });
        return;
    }

    // ======================== 网页取件：严格校验验证码 ========================
    // 必须存在未过期的验证码且与手机号、令牌绑定，错误次数超限即失效；
    // 校验通过后消费验证码并标记已验证，杜绝任意码绕过
    if (pathname === '/api/verify' && req.method === 'POST') {
        const body = await parseBody(req);
        const { token, phone, code } = body;

        if (!token || !phone || !code) {
            sendJSON(res, 400, { error: '缺少必要参数' });
            return;
        }

        const data = loadData();
        const record = data[token];

        if (!record) {
            sendJSON(res, 403, { error: '请先获取验证码' });
            return;
        }
        if (record.status === 'verified' || record.status === 'opened') {
            sendJSON(res, 409, { error: '已验证过', alreadyVerified: true });
            return;
        }
        if (!record.smsCodeHash || !record.smsExpire) {
            sendJSON(res, 403, { error: '请先获取验证码' });
            return;
        }
        if (Date.now() > record.smsExpire) {
            sendJSON(res, 403, { error: '验证码已过期，请重新获取' });
            return;
        }
        if (record.phone !== phone) {
            sendJSON(res, 403, { error: '手机号与验证码不匹配' });
            return;
        }
        if (hashCode(code) !== record.smsCodeHash) {
            record.smsAttempts = (record.smsAttempts || 0) + 1;
            if (record.smsAttempts >= SMS_MAX_ATTEMPTS) {
                record.smsCodeHash = null;
                record.smsExpire = 0;
                console.log('[验证失败] Token:', token, '错误次数超限，验证码已失效');
                saveData(data);
                sendJSON(res, 403, { error: '错误次数过多，请重新获取验证码' });
                return;
            }
            saveData(data);
            console.log('[验证失败] Token:', token, '验证码错误，剩余次数:', SMS_MAX_ATTEMPTS - record.smsAttempts);
            sendJSON(res, 403, { error: '验证码错误，剩余' + (SMS_MAX_ATTEMPTS - record.smsAttempts) + '次机会' });
            return;
        }

        /* 校验通过：消费验证码，标记已验证 */
        record.status = 'verified';
        record.createdAt = new Date().toISOString();
        record.smsCodeHash = null;
        record.smsExpire = 0;
        record.smsAttempts = 0;
        saveData(data);

        console.log('[验证成功] Token:', token, 'Phone:', phone);
        sendJSON(res, 200, { success: true, message: '验证成功' });
        return;
    }

    if (pathname === '/sms_key' && req.method === 'POST') {
        const body = await parseBody(req);
        const { token } = body;

        /* 按来源 IP 节流，防短信密码被高频抓取 */
        if (smsKeyThrottled(req.socket.remoteAddress || 'unknown')) {
            sendJSON(res, 429, { ok: false, msg: 'too many requests' });
            return;
        }

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
                verifiedAt: record.createdAt,
                remaining: record.remaining || []
            });
        } else if (record && record.status === 'opened') {
            sendJSON(res, 200, {
                verified: true,
                phone: record.phone,
                verifiedAt: record.createdAt,
                remaining: record.remaining || [],
                opened: true
            });
        } else {
            sendJSON(res, 200, { verified: false });
        }
        return;
    }

    if (pathname === '/api/update-remaining' && req.method === 'POST') {
        const body = await parseBody(req);
        const { token, remaining } = body;

        if (!token) {
            sendJSON(res, 400, { error: '缺少token参数' });
            return;
        }

        const data = loadData();
        const record = data[token];

        if (!record) {
            sendJSON(res, 404, { error: '令牌不存在' });
            return;
        }

        record.remaining = remaining || [];
        record.status = 'opened';
        saveData(data);

        console.log('[更新剩余包裹] Token:', token, '剩余:', JSON.stringify(remaining));
        sendJSON(res, 200, { success: true });
        return;
    }

    // ======================== 快递员账号 API ========================
    if (pathname === '/api/courier/check' && req.method === 'GET') {
        // 嵌入式端登录校验：手机号是否在快递员名单中（无需管理密钥）
        const phone = parsedUrl.query.phone;

        if (!phone) {
            sendJSON(res, 400, { error: '缺少phone参数' });
            return;
        }

        const valid = loadCouriers().includes(phone);
        console.log('[快递员校验] Phone:', phone, 'valid:', valid);
        sendJSON(res, 200, { valid });
        return;
    }

    if (pathname === '/api/courier/list' && req.method === 'GET') {
        if (!isCourierAdmin(req)) { sendJSON(res, 403, { error: 'auth failed' }); return; }
        sendJSON(res, 200, { phones: loadCouriers() });
        return;
    }

    if (pathname === '/api/courier/add' && req.method === 'POST') {
        if (!isCourierAdmin(req)) { sendJSON(res, 403, { error: 'auth failed' }); return; }
        const body = await parseBody(req);
        const phone = body.phone;

        if (!phone || !/^1\d{10}$/.test(phone)) {
            sendJSON(res, 400, { error: '手机号格式错误' });
            return;
        }

        const phones = loadCouriers();
        if (!phones.includes(phone)) phones.push(phone);
        saveCouriers(phones);
        console.log('[快递员添加] Phone:', phone);
        sendJSON(res, 200, { success: true, phones });
        return;
    }

    if (pathname === '/api/courier/remove' && req.method === 'POST') {
        if (!isCourierAdmin(req)) { sendJSON(res, 403, { error: 'auth failed' }); return; }
        const body = await parseBody(req);
        const phone = body.phone;

        if (!phone) {
            sendJSON(res, 400, { error: '缺少phone参数' });
            return;
        }

        const phones = loadCouriers().filter(p => p !== phone);
        saveCouriers(phones);
        console.log('[快递员移除] Phone:', phone);
        sendJSON(res, 200, { success: true, phones });
        return;
    }

    // ======================== 用户账号 API ========================
    if (pathname === '/api/user/check' && req.method === 'GET') {
        // 嵌入式端登录校验：手机号是否在用户名单中（无需管理密钥）
        const phone = parsedUrl.query.phone;

        if (!phone) {
            sendJSON(res, 400, { error: '缺少phone参数' });
            return;
        }

        const valid = loadUsers().includes(phone);
        console.log('[用户校验] Phone:', phone, 'valid:', valid);
        sendJSON(res, 200, { valid });
        return;
    }

    if (pathname === '/api/user/list' && req.method === 'GET') {
        if (!isUserAdmin(req)) { sendJSON(res, 403, { error: 'auth failed' }); return; }
        sendJSON(res, 200, { phones: loadUsers() });
        return;
    }

    if (pathname === '/api/user/add' && req.method === 'POST') {
        if (!isUserAdmin(req)) { sendJSON(res, 403, { error: 'auth failed' }); return; }
        const body = await parseBody(req);
        const phone = body.phone;

        if (!phone || !/^1\d{10}$/.test(phone)) {
            sendJSON(res, 400, { error: '手机号格式错误' });
            return;
        }

        const phones = loadUsers();
        if (!phones.includes(phone)) phones.push(phone);
        saveUsers(phones);
        console.log('[用户添加] Phone:', phone);
        sendJSON(res, 200, { success: true, phones });
        return;
    }

    if (pathname === '/api/user/remove' && req.method === 'POST') {
        if (!isUserAdmin(req)) { sendJSON(res, 403, { error: 'auth failed' }); return; }
        const body = await parseBody(req);
        const phone = body.phone;

        if (!phone) {
            sendJSON(res, 400, { error: '缺少phone参数' });
            return;
        }

        const phones = loadUsers().filter(p => p !== phone);
        saveUsers(phones);
        console.log('[用户移除] Phone:', phone);
        sendJSON(res, 200, { success: true, phones });
        return;
    }

    // ======================== 登录记录上报 API ========================
    // 嵌入式端用户登录成功后异步 POST 登录记录，不参与实时校验；
    // 记录追加进 user_data.json 的 records 数组（不覆盖 phones 名单）
    if (pathname === '/api/login/record' && req.method === 'POST') {
        const body = await parseBody(req);
        const phone = body.phone;

        if (!phone || !/^1\d{10}$/.test(phone)) {
            sendJSON(res, 400, { error: '手机号格式错误' });
            return;
        }

        const data = loadUserData();
        if (!Array.isArray(data.records)) data.records = [];
        data.records.push({
            phone,
            role: body.role || 'user',
            loginTime: typeof body.loginTime === 'number' ? body.loginTime : Date.now(),
            serverTime: Date.now()
        });
        saveUserData(data);
        console.log('[登录上报] Phone:', phone, 'role:', body.role, 'loginTime:', body.loginTime);
        sendJSON(res, 200, { success: true });
        return;
    }

    sendHTML(res, HTML_PAGE);
});

server.listen(3000, '0.0.0.0', () => {
    console.log('快递柜验证服务已启动: http://0.0.0.0:3000');
});