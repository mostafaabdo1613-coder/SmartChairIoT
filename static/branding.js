import { db, ref, onValue, set } from "/static/firebase.js";

function resizeToBase64(file, maxSize, cb) {
  const reader = new FileReader();
  reader.onload = (ev) => {
    const img = new Image();
    img.onload = () => {
      let w = img.width, h = img.height;
      if (w > h) { h = h * (maxSize / w); w = maxSize; } else { w = w * (maxSize / h); h = maxSize; }
      const canvas = document.createElement('canvas');
      canvas.width = w; canvas.height = h;
      canvas.getContext('2d').drawImage(img, 0, 0, w, h);
      cb(canvas.toDataURL('image/png', 0.85));
    };
    img.src = ev.target.result;
  };
  reader.readAsDataURL(file);
}

// ── الشعار في القائمة الجانبية ──
const brandIcon = document.getElementById('brand-icon');
if (brandIcon) {
  onValue(ref(db, 'app_settings/logo_base64'), snap => {
    const logo = snap.val();
    brandIcon.innerHTML = logo
      ? `<img src="${logo}" alt="شعار المشروع" style="width:100%;height:100%;object-fit:cover;border-radius:inherit;" />`
      : `<i class="ti ti-wheelchair" aria-hidden="true"></i>`;
  });

  brandIcon.style.cursor = 'pointer';
  brandIcon.title = 'اضغط لتغيير الشعار';
  brandIcon.addEventListener('click', () => {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = 'image/*';
    input.onchange = (e) => {
      const file = e.target.files[0];
      if (!file) return;
      resizeToBase64(file, 180, base64 => {
        set(ref(db, 'app_settings/logo_base64'), base64);
      });
    };
    input.click();
  });
}
