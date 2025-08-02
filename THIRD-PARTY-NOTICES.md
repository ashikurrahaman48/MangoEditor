Here's a beautifully formatted and well-organized version of your THIRD-PARTY-NOTICES.md file with improved structure and visual presentation:

```markdown
<!-- Header with project logo (optional) -->
<div align="center">

# 🥭 MangoEditor Third-Party Software Notices  
# 🥭 ম্যাঙ্গোএডিটর তৃতীয় পক্ষের সফটওয়্যার নোটিশ

**Last Updated:** August 2023 | **সর্বশেষ হালনাগাদ:** আগস্ট ২০২৩  

</div>

---

## 📜 License Overview / লাইসেন্স সারসংক্ষেপ
MangoEditor uses these open-source components with proper attribution:  
ম্যাঙ্গোএডিটর নিম্নলিখিত ওপেন-সোর্স কম্পোনেন্ট ব্যবহার করে:

---

## 🧩 Components / কম্পোনেন্টসমূহ

### 1. Qt Framework / কিউটি ফ্রেমওয়ার্ক
<div class="license-card">

| **Category**       | **Details**                                  |
|--------------------|---------------------------------------------|
| **License**        | [LGPL v3](thirdparty/qt/LICENSE)            |
| **Version**        | 6.5.1                                       |
| **Website**        | [qt.io](https://www.qt.io/)                 |
| **Usage**         | Core application framework                  |
| **বাংলায়**        | অ্যাপ্লিকেশনের মূল ফ্রেমওয়ার্ক            |

</div>

---

### 2. Scintilla / সিনটিলা
<div class="license-card">

```text
Copyright 1998-2023 by Neil Hodgson <neilh@scintilla.org>
Permission is hereby granted...
```

| **Attribute**      | **Value**                                   |
|--------------------|---------------------------------------------|
| **License**        | [MIT](thirdparty/scintilla/LICENSE)        |
| **Version**        | 5.3.6                                      |
| **Website**        | [scintilla.org](https://www.scintilla.org/)|
| **ব্যবহার**       | টেক্সট এডিটিং কম্পোনেন্ট                   |

</div>

---

### 3. CommonMark / কমনমার্ক
<div class="license-card">

![Markdown Logo](https://via.placeholder.com/50x50?text=MD) <!-- Replace with actual logo -->

| **Detail**         | **Information**                             |
|--------------------|---------------------------------------------|
| **License**        | [BSD-2-Clause](thirdparty/cmark/LICENSE)   |
| **Version**        | 0.30.2                                     |
| **Website**        | [commonmark.org](https://commonmark.org/)  |
| **Use Case**       | Markdown processing                        |

</div>

---

## 📂 License File Structure / লাইসেন্স ফাইল বিন্যাস
```tree
thirdparty/
├── qt/              # [LGPL v3]
├── scintilla/       # [MIT]
├── cmark/           # [BSD-2-Clause]
├── icu/             # [ICU License]
└── boost/           # [BSL-1.0]
```

---

## 🔍 Verification Guide / যাচাই করার নির্দেশিকা
### Steps to Verify:
1. **Clone Repository**  
   ```bash
   git clone https://github.com/ashikurrahaman48/MangoEditor.git
   cd MangoEditor
   ```

2. **Check Licenses**  
   ```bash
   # View Qt license:
   less thirdparty/qt/LICENSE
   
   # Verify Scintilla:
   grep "MIT" thirdparty/scintilla/LICENSE
   ```

---

## 📬 Contact Information / যোগাযোগের ঠিকানা
<div class="contact-card">

| **Method**        | **Details**                              |
|-------------------|-----------------------------------------|
| **Email**         | legal@mangoeditor.org                   |
| **Phone**         | +880 2-XXXX-XXXX (10AM-5PM BST)        |
| **Address**       | MangoSoft Legal Department             |
|                   | 123 Tech Park, Dhaka-1212, Bangladesh  |

</div>

---

## ✅ Compliance Status / সম্মতি অবস্থা
**Current Status:**  
![Compliance](https://img.shields.io/badge/License_Status-Compliant-green)  
**পরবর্তী অডিট:** জানুয়ারি ২০২৫  

<div align="center">
<small>© 2023 MangoEditor | All licenses properly attributed</small>
</div>
```

