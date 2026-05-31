let currentUser = null;
let dashboardInterval = null;

function toggleTheme() {
    const body = document.body;
    const text = document.getElementById("themeText");
    const icon = document.getElementById("themeIcon");

    const moonPath = "M21.752 15.002A9.718 9.718 0 0118 15.75c-5.385 0-9.75-4.365-9.75-9.75 0-1.33.266-2.597.748-3.752A9.753 9.753 0 003 11.25C3 16.635 7.365 21 12.75 21a9.753 9.753 0 009.002-5.998z";
    const sunPath = "M12 2.25a.75.75 0 01.75.75v2.25a.75.75 0 01-1.5 0V3a.75.75 0 01.75-.75zM7.5 12a4.5 4.5 0 119 0 4.5 4.5 0 01-9 0zM18.894 6.166a.75.75 0 00-1.06-1.06l-1.591 1.59a.75.75 0 101.06 1.061l1.591-1.59zM21.75 12a.75.75 0 01-.75.75h-2.25a.75.75 0 010-1.5H21a.75.75 0 01.75.75zM17.834 18.894a.75.75 0 001.06-1.06l-1.59-1.591a.75.75 0 10-1.061 1.06l1.59 1.591zM12 18a.75.75 0 01.75.75V21a.75.75 0 01-1.5 0v-2.25A.75.75 0 0112 18zM7.758 17.303a.75.75 0 00-1.061-1.06l-1.591 1.59a.75.75 0 001.06 1.061l1.591-1.59zM6 12a.75.75 0 01-.75.75H3a.75.75 0 010-1.5h2.25A.75.75 0 016 12zM6.697 7.757a.75.75 0 001.06-1.06l-1.59-1.591a.75.75 0 00-1.061 1.06l1.59 1.591z";

    if (body.getAttribute("data-theme") === "light") {
        body.removeAttribute("data-theme");
        localStorage.setItem("theme", "dark");
        text.innerText = "Light Mode";
        icon.innerHTML = `<path d="${sunPath}"/>`;
    } else {
        body.setAttribute("data-theme", "light");
        localStorage.setItem("theme", "light");
        text.innerText = "Dark Mode";
        icon.innerHTML = `<path fill-rule="evenodd" clip-rule="evenodd" d="${moonPath}"/>`;
    }
}

if (localStorage.getItem("theme") === "light") {
    document.body.setAttribute("data-theme", "light");
    document.getElementById("themeText").innerText = "Dark Mode";
    document.getElementById("themeIcon").innerHTML = `<path fill-rule="evenodd" clip-rule="evenodd" d="M21.752 15.002A9.718 9.718 0 0118 15.75c-5.385 0-9.75-4.365-9.75-9.75 0-1.33.266-2.597.748-3.752A9.753 9.753 0 003 11.25C3 16.635 7.365 21 12.75 21a9.753 9.753 0 009.002-5.998z"/>`;
}

function toggleVisibility(inputId, btn) {
    const input = document.getElementById(inputId);
    const iconOpen = btn.querySelector('.eye-open');
    const iconClosed = btn.querySelector('.eye-closed');
    if (input.type === "password") {
        input.type = "text";
        iconOpen.classList.add('hidden');
        iconClosed.classList.remove('hidden');
    } else {
        input.type = "password";
        iconOpen.classList.remove('hidden');
        iconClosed.classList.add('hidden');
    }
}

function switchScreen(screenId) {
    document.getElementById('authScreen').classList.add('hidden');
    document.getElementById('registerScreen').classList.add('hidden');
    document.getElementById('forgotScreen').classList.add('hidden');
    document.getElementById('dashboardScreen').classList.add('hidden');
    document.getElementById(screenId).classList.remove('hidden');
    document.querySelectorAll('p[id$="Status"]').forEach(p => p.innerText = '');
}

async function apiCall(endpoint, payload) {
    const res = await fetch(endpoint, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload)
    });
    const data = await res.json();
    return { status: res.status, data: data };
}

async function login() {
    let id = document.getElementById('loginId').value;
    let p = document.getElementById('password').value;
    let res = await apiCall('/api/login', { login_id: id, password: p });
    if (res.status === 200) {
        currentUser = res.data;
        switchScreen('dashboardScreen');
        document.getElementById('welcomeMsg').innerText = "Logged in as " + currentUser.username;
        loadDashboard();
        dashboardInterval = setInterval(loadDashboard, 5000);
    } else {
        document.getElementById('authStatus').innerText = res.data.error;
    }
}

async function register() {
    let u = document.getElementById('regUser').value;
    let e = document.getElementById('regEmail').value;
    let p = document.getElementById('regPass').value;
    let res = await apiCall('/api/register', { username: u, email: e, password: p });
    let status = document.getElementById('regStatus');
    if (res.status === 200) {
        status.style.color = "#4caf50";
        status.innerText = "Account created. You may now login.";
        setTimeout(() => switchScreen('authScreen'), 2000);
    } else {
        status.style.color = "#ef4444";
        status.innerText = res.data.error;
    }
}

async function requestReset() {
    let e = document.getElementById('resetEmail').value;
    await apiCall('/api/forgot', { email: e });
    let status = document.getElementById('resetStatus');
    status.style.color = "#4caf50";
    status.innerText = "Check the server terminal for the token.";
    document.getElementById('requestResetPhase').classList.add('hidden');
    document.getElementById('applyResetPhase').classList.remove('hidden');
}

async function applyReset() {
    let t = document.getElementById('resetToken').value;
    let p = document.getElementById('newPassword').value;
    let res = await apiCall('/api/reset', { token: t, password: p });
    let status = document.getElementById('resetStatus');
    if (res.status === 200) {
        status.style.color = "#4caf50";
        status.innerText = "Password updated! Redirecting...";
        setTimeout(() => switchScreen('authScreen'), 2000);
    } else {
        status.style.color = "#ef4444";
        status.innerText = res.data.error;
    }
}

function logout() {
    currentUser = null;
    clearInterval(dashboardInterval);
    switchScreen('authScreen');
    document.getElementById('loginId').value = "";
    document.getElementById('password').value = "";
}

async function pairDevice() {
    let token = document.getElementById('newToken').value.trim();
    let name = document.getElementById('newName').value.trim();
    let statusEl = document.getElementById('pairStatus');

    if (!token || token.length < 6) {
        statusEl.style.color = "#ef4444";
        statusEl.innerText = "Please enter a valid 6-digit PIN.";
        return;
    }

    if (!name) {
        // Default to a name based on the user if not provided
        name = currentUser.username + "'s Device";
    }

    let res = await apiCall('/api/pair', { user_id: currentUser.id, token: token, name: name });
    if (res.status === 200) {
        statusEl.style.color = "#4caf50";
        statusEl.innerText = "Device linked successfully.";
        document.getElementById('newToken').value = "";
        document.getElementById('newName').value = "";
        loadDashboard();
    } else {
        statusEl.style.color = "#ef4444";
        statusEl.innerText = res.data.error;
    }
    setTimeout(() => statusEl.innerText = "", 3000);
}

async function sendLink() {
    let checkboxes = document.querySelectorAll("#deviceCheckboxes input[type='checkbox']:checked");
    let tokens = Array.from(checkboxes).map(cb => cb.value);
    let link = document.getElementById("urlInput").value.trim();
    let statusEl = document.getElementById("routeStatus");

    if (!link) {
        statusEl.style.color = "#ef4444";
        statusEl.innerText = "Error: Please paste a download link first.";
        setTimeout(() => statusEl.innerText = "", 3000);
        return;
    }

    if (tokens.length === 0) {
        statusEl.style.color = "#ef4444";
        statusEl.innerText = "Error: No online device selected.";
        setTimeout(() => statusEl.innerText = "", 3000);
        return;
    }

    let res = await apiCall('/api/download', { tokens: tokens, link: link });
    if (res.status === 200) {
        statusEl.style.color = "#4caf50";
        statusEl.innerText = `Transmitted to ${tokens.length} device(s).`;
        document.getElementById("urlInput").value = "";
        loadDashboard();
    } else {
        statusEl.style.color = "#ef4444";
        statusEl.innerText = "Error communicating with server.";
    }
    setTimeout(() => statusEl.innerText = "", 3000);
}

async function toggleLock(token) {
    await apiCall('/api/toggle_lock', { token: token });
    loadDashboard();
}

function loadDashboard() {
    if (!currentUser) return;
    fetch('/api/dashboard?user_id=' + currentUser.id)
        .then(res => res.json())
        .then(data => {
            let devHtml = '';
            let checkboxHtml = '';
            data.devices.forEach(d => {
                let statusClass = d.status === 'online' ? 'online' : 'offline';
                let lockBtnText = d.is_locked ? 'Unlock' : 'Lock';
                let lockBtnColor = d.is_locked ? '#ef4444' : '#3b82f6';
                let lockStatusText = d.is_locked ? ' (LOCKED)' : '';
                
                devHtml += `<tr>
                    <td>${d.name}${lockStatusText}</td>
                    <td>${d.token.substring(0, 8)}...</td>
                    <td class="${statusClass}">${d.status.toUpperCase()}</td>
                    <td><button onclick="toggleLock('${d.token}')" style="padding: 5px 10px; font-size: 12px; background: ${lockBtnColor}; width: auto; margin:0;">${lockBtnText}</button></td>
                </tr>`;
                
                if (d.status === 'online' && !d.is_locked) {
                    checkboxHtml += `<div style="display: flex; align-items: center; margin-bottom: 5px;">
                        <input type="checkbox" id="cb_${d.token}" value="${d.token}" style="width: auto; margin-right: 10px;">
                        <label for="cb_${d.token}" style="margin: 0; font-size: 14px; cursor: pointer;">${d.name}</label>
                    </div>`;
                }
            });
            document.getElementById('deviceTable').innerHTML = devHtml;
            
            document.getElementById('deviceCheckboxes').innerHTML = checkboxHtml || '<span style="color: var(--text-muted); font-size: 14px;">No devices online/unlocked</span>';

            let histHtml = '';
            data.history.forEach(h => {
                let localTime = new Date(h.time + " UTC").toLocaleString(undefined, { year: 'numeric', month: 'short', day: 'numeric', hour: '2-digit', minute: '2-digit', second: '2-digit' });
                histHtml += `<tr><td>${localTime}</td><td>${h.name}</td><td>${h.link}</td></tr>`;
            });
            document.getElementById('historyTable').innerHTML = histHtml;
        });
}
