import { initializeApp } from "https://www.gstatic.com/firebasejs/10.12.0/firebase-app.js";
import {
  getDatabase, ref, onValue, set, push, remove, update
} from "https://www.gstatic.com/firebasejs/10.12.0/firebase-database.js";

const firebaseConfig = {
  apiKey: "AIzaSyDp40s_gR8xX5rNEMqYsxtqm1io8TQuEhE",
  authDomain: "smartchair-b40bb.firebaseapp.com",
  projectId: "smartchair-b40bb",
 
  databaseURL: "https://smartchair-b40bb-default-rtdb.firebaseio.com/",
  storageBucket: "smartchair-b40bb.firebasestorage.app",
  messagingSenderId: "848086421541",
  appId: "1:848086421541:web:70ca86201542d2363e6fcc"
};

const app = initializeApp(firebaseConfig);
const db  = getDatabase(app);



export { db, ref, onValue, set, push, remove, update };