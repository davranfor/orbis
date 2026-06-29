/* app.js — minimal CRUD demo for orbis server
 *
 * API endpoints used:
 *   POST   /api/login            body: { email, password }
 *   POST   /api/logout
 *   GET    /api/users            → [[id, name], ...]
 *   POST   /api/users            body: { id, role, name, email, password }
 *   PATCH  /api/users?id=<n>     body: { role, name, email }
 *   DELETE /api/users?id=<n>
 */

"use strict";

/* ── low-level fetch helper ──────────────────────────── */

async function api(method, path, body) {
    const opts = {
        method,
        headers: body ? { "Content-Type": "application/json" } : {},
        body:    body ? JSON.stringify(body) : undefined,
    };
    const res = await fetch(path, opts);
    const text = await res.text();
    return { ok: res.ok, status: res.status, body: text };
}

/* ── view helpers ────────────────────────────────────── */

function show(id)  { document.getElementById(id).classList.remove("hidden"); }
function hide(id)  { document.getElementById(id).classList.add("hidden"); }
function val(id)   { return document.getElementById(id).value.trim(); }
function setVal(id, v) { document.getElementById(id).value = v; }

function setMsg(id, text, type) {
    const el = document.getElementById(id);
    el.textContent = text;
    el.className = "msg " + type;
}

function clearMsg(id) {
    const el = document.getElementById(id);
    el.textContent = "";
    el.className = "msg";
}

/* ── login / logout ──────────────────────────────────── */

async function login() {
    clearMsg("login-msg");
    const email    = val("login-email");
    const password = val("login-password");

    if (!email || !password) {
        setMsg("login-msg", "Email and password are required.", "error");
        return;
    }

    const r = await api("POST", "/api/login", { email, password });

    if (!r.ok) {
        setMsg("login-msg", "Invalid credentials.", "error");
        return;
    }

    hide("view-login");
    show("view-users");
    show("btn-logout");
    loadUsers();
}

async function logout() {
    await api("POST", "/api/logout");
    location.reload(true);
}

/* ── load & render users ─────────────────────────────── */

function renderUsers(rows) {
    document.getElementById("users-tbody").innerHTML = rows.map(([id, name, email]) => `
        <tr>
            <td>${id}</td>
            <td>${escHtml(name)}</td>
            <td>${escHtml(email)}</td>
            <td>
                <button class="btn btn-warning" onclick="openEdit(${id}, '${escJs(name)}', '${escJs(email)}')">Edit</button>
                <button class="btn btn-danger"  onclick="deleteUser(${id})">Delete</button>
            </td>
        </tr>
    `).join("");
}

async function loadUsers() {
    const r = await api("GET", "/api/users");

    if (!r.ok) {
        document.getElementById("users-tbody").innerHTML =
            '<tr><td colspan="4">Failed to load users.</td></tr>';
        return;
    }
    renderUsers(JSON.parse(r.body));
}

/* ── create user ─────────────────────────────────────── */

async function createUser() {
    clearMsg("add-msg");

    const id       = parseInt(val("add-id"), 10);
    const role     = parseInt(val("add-role"), 10);
    const name     = val("add-name");
    const email    = val("add-email");
    const password = val("add-password");

    if (!id || !name || !email || !password) {
        setMsg("add-msg", "All fields are required.", "error");
        return;
    }

    const r = await api("POST", "/api/users", { id, role, name, email, password });

    if (!r.ok) {
        setMsg("add-msg", JSON.parse(r.body)?.title || "Error creating user.", "error");
        return;
    }

    setMsg("add-msg", "User added.", "success");
    ["add-id", "add-name", "add-email", "add-password"].forEach(f => setVal(f, ""));
    loadUsers();
}

/* ── edit user ───────────────────────────────────────── */

function openEdit(id, name, email) {
    setVal("edit-id", id);
    setVal("edit-name", name);
    setVal("edit-email", email);
    document.getElementById("edit-id-label").textContent = "#" + id;
    clearMsg("edit-msg");
    show("edit-panel");
    document.getElementById("edit-name").focus();
}

function closeEdit() {
    hide("edit-panel");
}

async function updateUser() {
    clearMsg("edit-msg");

    const id    = val("edit-id");
    const role  = parseInt(val("edit-role"), 10);
    const name  = val("edit-name");
    const email = val("edit-email");

    if (!name || !email) {
        setMsg("edit-msg", "Name and email are required.", "error");
        return;
    }

    const r = await api("PATCH", "/api/users?id=" + id, { role, name, email });

    if (!r.ok) {
        setMsg("edit-msg", JSON.parse(r.body)?.title || "Error updating user.", "error");
        return;
    }

    closeEdit();
    loadUsers();
}

/* ── delete user ─────────────────────────────────────── */

async function deleteUser(id) {
    if (!confirm("Delete user #" + id + "?")) return;

    const r = await api("DELETE", "/api/users?id=" + id);

    if (!r.ok) {
        alert("Error: " + (JSON.parse(r.body)?.title || r.body));
        return;
    }
    loadUsers();
}

/* ── utilities ───────────────────────────────────────── */

function escHtml(str) {
    return String(str)
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;");
}

function escJs(str) {
    return String(str).replace(/\\/g, "\\\\").replace(/'/g, "\\'");
}

/* ── enter key on login form ─────────────────────────── */

document.addEventListener("keydown", e => {
    if (e.key === "Enter" && !document.getElementById("view-login").classList.contains("hidden")) {
        login();
    }
});

/* ── check session on load ───────────────────────────── */

(async function init() {
    const r = await api("GET", "/api/users");

    if (r.ok) {
        hide("view-login");
        show("view-users");
        show("btn-logout");
        renderUsers(JSON.parse(r.body));
    }
}());
