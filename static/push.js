window.setupPushNotifications = async function(vapidPublicKey) {
    if (!('Notification' in window)) {
        alert('هذا المتصفح لا يدعم إشعارات الموبايل.');
        return;
    }

    const permission = await Notification.requestPermission();
    if (permission !== 'granted') {
        alert('تم رفض صلاحية الإشعارات من قِبل المستخدم.');
        return;
    }

    if (!('serviceWorker' in navigator)) {
        console.log('الـ Service Worker غير مدعوم في هذا المتصفح.');
        return;
    }

    try {
        const reg = await navigator.serviceWorker.ready;
        
        if (!vapidPublicKey) {
            alert('تم تفعيل الصلاحية بنجاح! (لم يتم ربط مفتاح VAPID في الخلفية بعد)');
            return;
        }

        const sub = await reg.pushManager.subscribe({
            userVisibleOnly: true,
            applicationServerKey: urlBase64ToUint8Array(vapidPublicKey)
        });

        await fetch('/save-subscription', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(sub)
        });

        alert('تم تفعيل إشعارات الموبايل بنجاح وربطها بالسيرفر! ✓');

    } catch (err) {
        console.error('فشل في الاشتراك في خدمة الـ Push:', err);
    }
};

function urlBase64ToUint8Array(base64String) {
    const padding = '='.repeat((4 - base64String.length % 4) % 4);
    const base64 = (base64String + padding).replace(/\-/g, '+').replace(/_/g, '/');
    const rawData = window.atob(base64);
    const outputArray = new Uint8Array(rawData.length);
    for (let i = 0; i < rawData.length; ++i) {
        outputArray[i] = rawData.charCodeAt(i);
    }
    return outputArray;
}