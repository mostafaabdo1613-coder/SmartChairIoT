from flask import Flask, render_template, request, jsonify, redirect, url_for, session
from apscheduler.schedulers.background import BackgroundScheduler
import json
import datetime
import requests
import hashlib

try:
    from pywebpush import webpush, WebPushException
    PUSH_AVAILABLE = True
except ImportError:
    PUSH_AVAILABLE = False

app = Flask(__name__)

# مفتاح سري لتشغيل جلسات العمل وحفظ حالة تسجيل الدخول للمستخدم
app.secret_key = "smart_chair_secure_secret_key_2026"

# هذا السطر مهم جداً لـ Vercel ليتمكن من تشغيل التطبيق بنجاح
application = app

FIREBASE_DB_URL = "https://smartchair-b40bb-default-rtdb.firebaseio.com"

VAPID_PRIVATE_KEY = "70tqMfUDUwnhn2svVIHjhADfQaxM3qnFl9v3hu1NDGE"
VAPID_PUBLIC_KEY  = "VoWg7Ig_BFeyAvonuxpt5mhZgmuUFNW6KRCYoKF-iYsHkhWczIw5Vd_k9ZvAQYgmxLP9RLKJKaXfosiedM2OBVI"
VAPID_CLAIMS = {"sub": "mailto:admin@smartchair.local"}

SUBSCRIPTIONS_FILE = "/tmp/smartchair_subscriptions.json"
_sent_today = {}  

# =========================================================
#  🔐 كلمة السر المخصصة والثابتة للأدمن (تستطيع تغييرها من هنا)
# =========================================================
ADMIN_SECRET_PASSWORD = "admin" 


def fb_get(path):
    try:
        r = requests.get(f"{FIREBASE_DB_URL}/{path}.json", timeout=5)
        return r.json() if r.ok else None
    except Exception:
        return None


def fb_put(path, value):
    try:
        requests.put(f"{FIREBASE_DB_URL}/{path}.json", json=value, timeout=5)
    except Exception:
        pass


def fb_push(path, value):
    try:
        requests.post(f"{FIREBASE_DB_URL}/{path}.json", json=value, timeout=5)
    except Exception:
        pass


def load_subscriptions():
    try:
        with open(SUBSCRIPTIONS_FILE) as f:
            return json.load(f)
    except Exception:
        return []


def save_subscription(sub):
    subs = load_subscriptions()
    if sub not in subs:
        subs.append(sub)
        try:
            with open(SUBSCRIPTIONS_FILE, "w") as f:
                json.dump(subs, f)
        except Exception:
            pass


def send_push_to_all(title, body):
    if not PUSH_AVAILABLE or not VAPID_PRIVATE_KEY:
        return
    for sub in load_subscriptions():
        try:
            webpush(
                subscription_info=sub,
                data=json.dumps({"title": title, "body": body}),
                vapid_private_key=VAPID_PRIVATE_KEY,
                vapid_claims=VAPID_CLAIMS,
                )
        except WebPushException:
            pass


def check_medication_schedule():
    meds = fb_get("medications") or {}
    now = datetime.datetime.now()
    cur_time = now.strftime("%H:%M")
    today_key = now.strftime("%Y-%m-%d")

    for med_id, med in meds.items():
        times = med.get("times", [])
        if cur_time not in times:
            continue
        dedup_key = f"{med_id}_{cur_time}_{today_key}"
        if _sent_today.get(dedup_key):
            continue
        _sent_today[dedup_key] = True

        name = med.get("name", "دواء")
        dose = med.get("dose", "")

        fb_put("config/trigger_buzzer_med", True)   

        fb_push("alerts", {
            "type": "warn",
            "message": f" موعد دواء {name} ({dose}) — تم تشغيل المنبه في الكرسي",
            "time": cur_time
        })

        send_push_to_all("موعد دواء ", f"{name} — {dose}")

        try:
            scheduler.add_job(
                confirm_check, "date",
                run_date=now + datetime.timedelta(minutes=30),
                args=[name]
            )
        except Exception:
            pass


def confirm_check(med_name):
    fb_push("alerts", {
        "type": "danger",
        "message": f" لم يتم تأكيد أخذ دواء {med_name} خلال 30 دقيقة من الموعد",
        "time": datetime.datetime.now().strftime("%H:%M")
    })
    send_push_to_all(" تنبيه متابعة دواء", f"لم يتم تأكيد أخذ {med_name}")


# تفعيل الـ Scheduler فقط إذا كان يعمل محلياً لضمان عدم انهيار سيرفر Vercel السحابي
scheduler = BackgroundScheduler()
try:
    scheduler.add_job(check_medication_schedule, "interval", seconds=20)
    scheduler.start()
except Exception:
    pass

# دالة حماية للتأكد من تسجيل الدخول
def check_auth():
    return 'username' in session


# --- مسارات واجهات العرض المحمية بنظام الحسابات الجديد ---

@app.route('/')
def index():
    if not check_auth(): return redirect(url_for('login'))
    return render_template('index.html', active='dashboard', vapid_public_key=VAPID_PUBLIC_KEY, role=session.get('role', 'user'))

@app.route('/patient')
def patient():
    if not check_auth(): return redirect(url_for('login'))
    return render_template('patient.html', active='patient', role=session.get('role', 'user'))

@app.route('/medications')
def medications():
    if not check_auth(): return redirect(url_for('login'))
    return render_template('medications.html', active='medications', vapid_public_key=VAPID_PUBLIC_KEY, role=session.get('role', 'user'))

@app.route('/alerts')
def alerts():
    if not check_auth(): return redirect(url_for('login'))
    return render_template('alerts.html', active='alerts', role=session.get('role', 'user'))

@app.route('/cron/check', methods=['GET', 'POST'])
def cron_check():
    check_medication_schedule()
    return jsonify({"status": "checked", "time": datetime.datetime.now().strftime("%H:%M")})

@app.route('/save-subscription', methods=['POST'])
def save_sub():
    save_subscription(request.get_json())
    return jsonify({"status": "ok"})


# =========================================================
# 🔐 بوابات الحماية الحديدية لمنع اليوزر العادي من التعديل 🔐
# =========================================================

@app.route('/api/update_safety', methods=['POST'])
def update_safety():
    """حماية سويتش نظام الأمان الطبي على السيرفر"""
    if not check_auth() or session.get('role') != 'admin':
        return jsonify({"status": "error", "message": "عذراً، لا تملك صلاحية تعديل نظام الأمان!"}), 403
    
    data = request.get_json() or {}
    enabled = data.get('enabled', False)
    fb_put("config/pulse_check_enable", enabled)
    return jsonify({"status": "success", "enabled": enabled})


@app.route('/api/manage_medication', methods=['POST'])
def manage_medication():
    """حماية إضافة وحذف الأدوية على السيرفر"""
    if not check_auth() or session.get('role') != 'admin':
        return jsonify({"status": "error", "message": "عذراً، حسابك للمشاهدة فقط ولا يمكنك التعديل في الأدوية!"}), 403
    
    data = request.get_json() or {}
    action = data.get('action') # 'add' أو 'delete'
    
    if action == 'add':
        med_id = data.get('id')
        med_info = data.get('med_data')
        fb_put(f"medications/{med_id}", med_info)
    elif action == 'delete':
        med_id = data.get('id')
        fb_put(f"medications/{med_id}", None) # مسحه من الفايربيز
        
    return jsonify({"status": "success"})


@app.route('/api/update_patient_profile', methods=['POST'])
def update_patient_profile():
    """حماية تعديل الملف الطبي والبيانات الشخصية على السيرفر"""
    if not check_auth() or session.get('role') != 'admin':
        return jsonify({"status": "error", "message": "عذراً، تعديل البيانات الطبية متاح فقط للآدمن!"}), 403
    
    data = request.get_json() or {}
    fb_put("patient", data)
    return jsonify({"status": "success"})


# =========================================================
#  🔐 مسارات الـ Username والـ Password المقفلة حديدياً للأدمن 🔐
# =========================================================

@app.route('/register', methods=['GET', 'POST'])
def register():
    if request.method == 'POST':
        username = request.form.get('username', '').strip().lower()
        password = request.form.get('password', '')
        
        if not username or not password:
            return render_template('register.html', error="برجاء ملء جميع الحقول")
            
        # ❌ حظر نهائي لأي محاولة تسجيل باسم admin من الصفحة الخارجية
        if username == "admin":
            return render_template('register.html', error="عذراً، حساب المسؤول (admin) محمي ومبني داخل النظام ولا يمكن إعادة تسجيله!")
            
        # التحقق إن كان اسم المستخدم مسجل مسبقاً في Firebase للمستخدمين العاديين
        existing_user = fb_get(f"app_users/{username}")
        if existing_user:
            return render_template('register.html', error="اسم المستخدم هذا محجوز بالفعل!")
            
        # أي شخص يسجل يصبح يوزر عادي تلقائياً وبشكل إجباري لحماية الكرسي
        role = "user"  
            
        # تشفير كلمة المرور للمستخدم العادي
        hashed_password = hashlib.sha256(password.encode()).hexdigest()
        
        # حفظ الحساب الجديد في Firebase
        fb_put(f"app_users/{username}", {"password": hashed_password, "role": role})
        return redirect(url_for('login', success_msg="تم إنشاء حسابك بنجاح! يمكنك الدخول الآن."))
        
    return render_template('register.html')


@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        username = request.form.get('username', '').strip().lower()
        password = request.form.get('password', '')
        
        # 1. مطابقة حساب الأدمن برمجياً دون تلاعب من الفايربيز
        if username == "admin":
            if password == ADMIN_SECRET_PASSWORD:
                session['username'] = "admin"
                session['role'] = "admin"
                return redirect(url_for('index'))
            else:
                return render_template('login.html', error="كلمة مرور المسؤول (Admin) غير صحيحة!")
        
        # 2. التحقق من بقية المستخدمين العاديين عبر Firebase
        user_data = fb_get(f"app_users/{username}")
        if user_data:
            hashed_password = hashlib.sha256(password.encode()).hexdigest()
            if user_data.get('password') == hashed_password:
                session['username'] = username
                session['role'] = user_data.get('role', 'user')
                return redirect(url_for('index'))
                
        return render_template('login.html', error="اسم المستخدم أو كلمة المرور غير صحيحة")
    return render_template('login.html', success_msg=request.args.get('success_msg'))


@app.route('/logout')
def logout():
    session.clear()
    return redirect(url_for('login'))


if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0', port=5000)