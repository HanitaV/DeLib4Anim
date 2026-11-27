# Vietnamese STEM Voice & Anti-AI-Slop Humanizer

Spoken science scripts fail when they sound like a translated Wikipedia page or a chatbot summary. Use this guide in Step 5 on every spoken (`>`) line.

---

## 1. Pronoun & Register Contract (Vietnamese Default)

- **Speaker -> Viewer:** `mình` -> `bạn` (warm, one-on-one peer conversation), or `chúng ta` when inviting the viewer to reason together (*"Trước tiên chúng ta cần hỏi..."*). Use `chúng mình` only in the outro when referring to the production team.
- **Avoid:** `tôi – quý vị`, `các bạn học sinh thân mến`, `chúng ta hãy cùng nhau...`, robotic `bạn đọc`, or generic `người ta` when `bạn` works better (*"bạn thử đẩy chiếc tủ"* beats *"người ta tác dụng lực lên vật"*).
- **Localize Everyday Anchors:** Prefer `xe máy`, `tuyến Hà Nội – Hải Phòng`, `bùng binh`, `tầng sáu`, `cốc cà phê trên tàu`, `đẩy chiếc tủ`, `đèn neon`, `sợi dây đàn guitar`.

---

## 2. Vietnamese & English Anti-AI-Slop Watchlist

Strip or rewrite every pattern below on first sight:

| # | AI Tell (Vietnamese & English) | Why It Fails in Spoken Video | Before -> After Rewrite |
| :--- | :--- | :--- | :--- |
| 1 | **Not X, but Y** (*"Không chỉ X, mà còn Y"*, *"Đây không phải là X, mà là Y"*, *"It's not just X, it's Y"*) | Inflates a simple fact with fake drama | *"Nó không chỉ là một phương trình, mà là chiếc kính hiển vi."* -> *"Phương trình Schrödinger chính là chiếc kính hiển vi thật sự."* |
| 2 | **Staged Run-Up** (*"Hãy cùng khám phá"*, *"Hãy cùng đi sâu tìm hiểu"*, *"Bây giờ chúng ta hãy bắt đầu hành trình"*, *"Let's dive in"*) | Delays the point; sounds like a presenter stalling | *"Hãy cùng khám phá xem gia tốc là gì."* -> *"Vận tốc cho biết vị trí thay đổi nhanh hay chậm. Theo đúng logic đó, gia tốc cho biết vận tốc thay đổi nhanh hay chậm."* |
| 3 | **One-Line Dramatic Fragments** (*"Đơn giản vậy thôi."*, *"Và mọi thứ đã thay đổi."*, *"Sự thật gây sốc."*) | Repeats the point without adding a new fact | Cut the fragment or merge its concrete consequence into the previous sentence. |
| 4 | **Inflated Significance** (*"đóng vai trò then chốt"*, *"minh chứng sống động"*, *"bức tranh toàn cảnh"*, *"nền tảng vững chắc"*, *"pivotal moment"*, *"testament"*) | Textbook filler words nobody says out loud | *"Lực pháp tuyến đóng vai trò quan trọng trong cảm giác trọng lượng."* -> *"Chính lực pháp tuyến mới là thứ cho chúng ta cảm giác về trọng lượng."* |
| 5 | **Forced Triads** (grouping 3 abstract nouns for rhythm: *"nhanh chóng, chính xác và hiệu quả"*) | Signals template prose over real thought | Keep only the specific items the physics actually requires. |
| 6 | **Em-Dashes (`—`) in Spoken Lines** | Listeners cannot hear punctuation; models overuse dashes to avoid choosing a clear connector | Replace `—` with a comma, period, or `[..]` pause. |
| 7 | **Textbook Passive Voice** (*"Chuyển động được mô tả bởi..."*, *"Gia tốc được định nghĩa là..."*) | Hides the actor and puts listeners to sleep | *"Lực được chia cho khối lượng để tìm gia tốc."* -> *"Lấy lực chia cho khối lượng, bạn ra được gia tốc."* |
| 8 | **Empty Deictics** (*"Như bạn thấy trên màn hình"*, *"Nhìn vào mũi tên này"*, *"As you can see here"*) | Fails the audio-only blind test and breaks auto-captions | *"Như bạn thấy ở đây, độ lớn tăng gấp đôi."* -> *"Khi đẩy mạnh gấp đôi, chiếc xe tăng tốc nhanh gấp đôi."* |
| 9 | **Fake Objections** (*"Một số người có thể cho rằng..."*, *"Some might say..."*) | Argues with a strawman instead of the viewer | Use *"Có một chỗ dễ gây nhầm lẫn ở đây..."* only for real student misconceptions. |
| 10 | **Chatbot Wrappers** (*"Tuyệt vời!"*, *"Câu hỏi rất hay!"*, *"Hy vọng video này hữu ích..."*) | Residual chat tone | Delete completely. |
| 11 | **Over-stacked Hedges** (*"có lẽ phần nào có thể tạm hiểu là..."*) | Sounds unsure and bloated | State the scope cleanly once (*"với góc nhỏ..."*, *"gần mặt đất nếu bỏ qua không khí..."*). |
| 12 | **Raw Math / LaTeX in VO** (`$F = ma$`, `\vec{P}`, `m/s^2`) | TTS and readers stumble or read syntax verbatim | Write spoken words: `mét trên giây bình phương`, `F bằng m nhân a`, `căn bậc hai`. |

---

## 3. Spoken Punctuation, Pauses & Stress Markers

Use consistent lightweight tags so both human voice actors and TTS preprocessors time beats cleanly:

| Marker | Meaning | Duration Budget | When to Use |
| :--- | :--- | :--- | :--- |
| `[.]` | Short beat | `0.4s` | Between a question and its answer, or short punchy statements |
| `[..]` | Medium pause | `0.8s` | Before a surprising number or pivot (*"cùng một lúc [..] Vậy điều gì..."*) |
| `[...]` | Long visual absorption | `1.5s` | Right after revealing a complex graph, formula card, or 3D orbital |
| `[BEAT]` | Major chapter / music breath | `2.5s` | Chapter transition or dramatic measurement collapse |
| `**word**` | Vocal stress | — | At most 1–2 stressed words per paragraph (*"đo **đồng thời**"*, *"tại **một** điểm"*) |

### Breath & Readability Rules
- **One-Breath Cap:** Keep spoken sentences under `35` words (`<= 38` hard lint threshold). Split long compound sentences at natural causal joints.
- **Vary Sentence Length:** Alternate a 20-word explanation with a 6-word realization (*"Nó không bao giờ chạm đất."*).
- **Pronunciation Hints (Glossary Only):** Keep normal spelling in the script or clean Vietnamese phonetics only where TTS misreads (`Schrödinger` -> note `/ˈʃrøːdɪŋər/` in Glossary; `Heisenberg` -> note in Glossary; `orbita` / `obitan` keep consistent across script).

---

## 4. Vietnamese Caption / Subtitle Line-Break Rules (For Downstream Sync)

- **Max 2 lines** on screen simultaneously, **`<= 42` characters per line** (`~15–42` chars comfortable reading speed).
- **Never split** inside a compound term (`gia / tốc`, `hệ quy / chiếu`, `nguyên lý / bất định`, `mômen / động lượng`).
- Break at natural comma/clause boundaries so muted viewers read each phrase in a single eye fixation.
