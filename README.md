```markdown
<div align="center">
  <img src="docs/images/logo.png" width="150" alt="MangoEditor Logo">
  <h1>🥭 MangoEditor - আধুনিক কোড এডিটর</h1>
  
  [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
  [![Build Status](https://github.com/mangoeditor/mangoeditor/actions/workflows/build.yml/badge.svg)](https://github.com/mangoeditor/mangoeditor/actions)
  [![Downloads](https://img.shields.io/github/downloads/mangoeditor/mangoeditor/total.svg)](https://github.com/mangoeditor/mangoeditor/releases)
  
  <img src="docs/images/screenshot.png" width="800" alt="MangoEditor Screenshot">
</div>

## ✨ প্রধান বৈশিষ্ট্যসমূহ

<div class="features">
  
| **কোডিং**           | **ইন্টিগ্রেশন**       | **ইন্টারফেস**        |
|---------------------|-----------------------|----------------------|
| 🟢 ৫০+ ভাষার সাপোর্ট | 🔗 গিট ইন্টিগ্রেশন    | 🌙 ডার্ক/লাইট মোড  |
| 🧠 স্মার্ট কমপ্লিশন     | 🖥️ টার্মিনাল         | 🎨 থিম কাস্টমাইজেশন |
| 📑 স্প্লিট ভিউ        | 📦 প্লাগইন সিস্টেম    | 🇧🇩 বাংলা ইন্টারফেস |
| 🔍 রেগুলার এক্সপ্রেশন | 📊 ডিবাগার          | 🛠️ কীবাইন্ড এডিটর  |

</div>

## 🚀 ইনস্টলেশন

### 📥 প্রি-বিল্ড প্যাকেজ

```bash
# Linux (Debian/Ubuntu)
sudo apt install ./mangoeditor_1.2.0_amd64.deb

# Windows
winget install MangoEditor.MangoEditor

# macOS
brew install mangoeditor
```

### 🔨 সোর্স থেকে বিল্ড

**প্রয়োজনীয়তা:**
- CMake 3.15+
- Qt 5.15+
- C++17 কম্পাইলার

```bash
git clone https://github.com/mangoeditor/mangoeditor.git
cd mangoeditor
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --parallel $(nproc)
```

## 📚 ডকুমেন্টেশন

[![Documentation](https://img.shields.io/badge/View-Documentation-blue)](https://docs.mangoeditor.org)

- [ইউজার গাইড](docs/user_manual.md)
- [API রেফারেন্স](docs/api_reference.md)
- [প্লাগইন ডেভেলপমেন্ট](docs/plugins.md)

## 🤝 কন্ট্রিবিউট করুন

আমরা ওপেন সোর্স কমিউনিটিকে স্বাগত জানাই! কিভাবে কন্ট্রিবিউট করবেন:

1. ইস্যু খুলুন বা ফিচার রিকুয়েস্ট করুন
2. ফর্ক করে পুল রিকুয়েস্ট পাঠান
3. আমাদের [কন্ট্রিবিউটিং গাইড](CONTRIBUTING.md) পড়ুন

[![Open in Gitpod](https://gitpod.io/button/open-in-gitpod.svg)](https://gitpod.io/#https://github.com/mangoeditor/mangoeditor)

## 📜 লাইসেন্স

MangoEditor [MIT লাইসেন্স](LICENSE) এর অধীনে প্রকাশিত।  
তৃতীয় পক্ষের লাইসেন্স: [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)

## 📬 যোগাযোগ

<div align="center">

| মাধ্যম        | লিংক/ঠিকানা                      |
|---------------|----------------------------------|
| 🌐 ওয়েবসাইট  | [mangoeditor.org](https://mangoeditor.org) |
| 📧 ইমেইল      | info@mangoeditor.org            |
| 💬 ডিসকর্ড    | [জয়েন করুন](https://discord.gg/mangoeditor) |
| 📱 ফেসবুক     | [পেজ লাইক করুন](https://facebook.com/mangoeditor) |

</div>

---

<div align="center">
  
**সর্বশেষ আপডেট:** আগস্ট ২০২৩  
[![Star on GitHub](https://img.shields.io/github/stars/mangoeditor/mangoeditor.svg?style=social)](https://github.com/mangoeditor/mangoeditor/stargazers)

</div>
```
