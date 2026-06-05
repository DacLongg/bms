# GIT BRANCH CHEAT SHEET

## 1. Kiểm tra trạng thái hiện tại

```bash
git status
```

Ý nghĩa:

* Kiểm tra file đã sửa, file chưa commit.
* Xem đang đứng ở nhánh nào.

---

## 2. Xem danh sách nhánh

### Chỉ xem local branch

```bash
git branch
```

Ví dụ:

```bash
* main
  develop
  feature/usb
```

Dấu `*` là nhánh hiện tại.

### Xem cả local và remote

```bash
git branch -a
```

---

## 3. Chuyển sang nhánh khác

### Cách mới (khuyến nghị)

```bash
git switch develop
```

### Cách cũ

```bash
git checkout develop
```

---

## 4. Chuyển về nhánh trước đó

```bash
git switch -
```

Ví dụ:

```bash
git switch main
git switch feature_usb

git switch -
```

→ quay lại main

---

## 5. Tạo nhánh mới và chuyển sang luôn

### Git mới

```bash
git switch -c feature/new_function
```

### Git cũ

```bash
git checkout -b feature/new_function
```

---

## 6. Đổi tên nhánh hiện tại

```bash
git branch -m ten_moi
```

Ví dụ:

```bash
git branch -m feature_usb
```

---

## 7. Đổi tên nhánh khác

```bash
git branch -m ten_cu ten_moi
```

Ví dụ:

```bash
git branch -m master main
```

---

## 8. Xóa nhánh local

### Đã merge

```bash
git branch -d feature_usb
```

### Chưa merge (ép xóa)

```bash
git branch -D feature_usb
```

---

## 9. Tải branch mới từ remote

```bash
git fetch
```

Ý nghĩa:

* Đồng bộ danh sách branch mới từ GitLab/GitHub.
* Không merge code.

---

## 10. Tạo local branch từ remote branch

Ví dụ remote có:

```bash
origin/feature_usb
```

Tạo local branch:

```bash
git switch -c feature_usb origin/feature_usb
```

Hoặc:

```bash
git checkout -b feature_usb origin/feature_usb
```

---

## 11. Đẩy branch lên remote

Lần đầu:

```bash
git push -u origin feature_usb
```

Các lần sau:

```bash
git push
```

---

## 12. Kéo code mới nhất

```bash
git pull
```

Tương đương:

```bash
git fetch
git merge
```

---

## 13. Kiểm tra lịch sử commit

```bash
git log --oneline
```

Ví dụ:

```bash
a1b2c3d Fix USB issue
d4e5f6g Update BIOS config
```

---

## 14. Chuyển về commit cũ để xem

```bash
git checkout <commit_id>
```

Ví dụ:

```bash
git checkout a1b2c3d
```

Lúc này ở trạng thái Detached HEAD.

Quay lại branch:

```bash
git switch main
```

---

## 15. Các lệnh dùng nhiều nhất hằng ngày

### Xem trạng thái

```bash
git status
```

### Xem branch

```bash
git branch
```

### Chuyển branch

```bash
git switch ten_branch
```

### Quay lại branch trước

```bash
git switch -
```

### Kéo code mới

```bash
git pull
```

### Đẩy code

```bash
git push
```

### Xem commit

```bash
git log --oneline
```

### Đồng bộ branch mới từ server

```bash
git fetch
```
