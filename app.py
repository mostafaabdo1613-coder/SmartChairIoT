from flask import Flask, render_template, request, jsonify
from apscheduler.schedulers.background import BackgroundScheduler
import json
import datetime
import requests
try:
    from pywebpush import webpush, WebPushException
    PUSH_AVAILABLE = True
except ImportError:
    PUSH_AVAILABLE = False

app = Flask(__name__)

# هذا السطر مهم جداً لـ Vercel ليتمكن من تشغيل التطبيق بنجاح
application = app

FIREBASE_DB_URL = "https://smartchair-b40bb-default-rtdb.firebaseio.com"

VAPID_PRIVATE_KEY = "70tqMfUDUwnhn2svVIHjhADfQaxM3qnFl9v3hu1NDGE"
VAPID_PUBLIC_KEY  = "VoWg7Ig_BFeyAvonuxpt5mhZgmuUFNW6KRCYoKF-iYsHkhWczIw5Vd_k9ZvAQYgmxLP9RLKJKaXfosiedM2OBVI"
VAPID_CLAIMS = {"sub": "mailto:admin@smartchair.local"}

SUBSCRIPTIONS_FILE = "/tmp/smartchair_subscriptions.json"
_sent_today = {}  


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

@app.route('/')
def index():
    return render_template('index.html', active='dashboard', vapid_public_key=VAPID_PUBLIC_KEY)

@app.route('/patient')
def patient():
    return render_template('patient.html', active='patient')

@app.route('/medications')
def medications():
    return render_template('medications.html', active='medications', vapid_public_key=VAPID_PUBLIC_KEY)

@app.route('/alerts')
def alerts():
    return render_template('alerts.html', active='alerts')

# راوت جديد مخصص لـ Vercel لفحص المواعيد أوتوماتيكياً عند تصفح الموقع أو عبر الـ ESP32
@app.route('/cron/check', methods=['GET', 'POST'])
def cron_check():
    check_medication_schedule()
    return jsonify({"status": "checked", "time": datetime.datetime.now().strftime("%H:%M")})

@app.route('/save-subscription', methods=['POST'])
def save_sub():
    save_subscription(request.get_json())
    return jsonify({"status": "ok"})


if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0', port=5000)