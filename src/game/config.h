#pragma once
// Texts the owner edits — port of web/config.js (title, subtitles, bird / cat lines)
// plus the zone names (ZONES) and time-of-day labels (TIME_INFO) from main.js.
#include <map>
#include <string>
#include <vector>

namespace config {

inline const std::string herName = "Patty";
inline const std::string title = "Patty's Winter Wonderland";
inline const std::string subtitle =
    "คืนคริสต์มาสอีฟ เดินเล่นในหมู่บ้านหิมะกับแชม จุ๋ย และน้องหมา ปีนภูเขาออโรร่าไปดูวิว แล้วลองแอบเข้าไปในถ้ำคริสตัลดูนะ";

struct Lines {
    std::vector<std::string> talk, lost, eat;
};

inline const std::map<std::string, Lines>& birds() {
    static const std::map<std::string, Lines> b = {
        {"cham",
         {{"หิวข้าวๆ", "รักนะ ♥", "Patty~", "แชมอยู่นี่!", "ขอเชอร์รี่หน่อย", "กรี๊ดดด", "ไปไหนกัน?", "อยู่ด้วยกันนะ"},
          {"Patty อยู่ไหน~", "ช่วยด้วยๆ", "แชมหลงทาง~", "หนาวจัง…"},
          {"อร่อย! ♥", "รักนะ ♥", "อีกๆ!", "อิ่มแล้ว~"}}},
        {"juice",
         {{"หิวข้าวๆ", "รักนะ ♥", "จุ๋ยมาแล้ว!", "Patty~", "หนาวจัง", "กรี๊ด!", "ขอเมล็ดทานตะวัน", "จุ๋ยรัก Patty"},
          {"หิว…", "Patty~ มารับหน่อย", "จุ๋ยอยู่นี่~"},
          {"อร่อยจัง ♥", "รักนะ ♥", "หม่ำๆ", "อีกลูก!"}}},
    };
    return b;
}

// Ter's cat (key "slid")
inline const Lines cat = {{"เมี้ยว~", "เมี้ยวว", "หิวปลา", "ง่วง…", "Ter~", "เมี้ยว ♥", "เฮงๆ อยู่นี่~"},
                          {"เมี้ยว…", "Zzz…", "เฮงๆ ง่วง…"},
                          {"งั่มๆ", "เมี้ยว ♥"}};

// cheer is commented out in config.js, so main.js falls back to this
inline const std::vector<std::string> cheer = {"เก่งมาก!"};

inline std::string subtitleFor(const std::string& character) {
    if (character == "ter") return "เฮงๆ หายไปไหน? ลองเดินหาในหมู่บ้านหิมะดูนะ";
    if (character == "momonga") return "โมโมงะออกเดินเล่น~ กระโดดกลางอากาศแล้วกางปีกร่อนได้เลย";
    return subtitle;
}

// place names (ZONES in main.js)
inline const std::map<std::string, std::pair<std::string, std::string>>& zones() {
    static const std::map<std::string, std::pair<std::string, std::string>> z = {
        // winter wonderland
        {"square", {"Village Square", "◆ ลานหมู่บ้าน — ที่ที่ทุกการผจญภัยเริ่มต้น"}},
        {"woods", {"Whispering Pines", "◆ ป่าสนกระซิบ"}},
        {"lake", {"Frozen Lake", "◆ ทะเลสาบน้ำแข็ง — ลื่นนะ"}},
        {"hill", {"Starlight Hill", "◆ เนินดูดาว"}},
        {"steps", {"Stone Steps", "◆ บันไดหิน"}},
        {"kennel", {"Puppy's Corner", "◆ มุมบ้านน้องหมา"}},
        {"home", {"Home", "◆ ห้องคริสต์มาสหลังเดิม"}},
        {"meadow", {"Meadow & Plains", "◆ ทุ่งหญ้าและเนินเขา"}},
        {"mountain", {"Mount Aurora", "◆ ภูเขาออโรร่า — กระโดดขึ้นทีละขั้น"}},
        {"summit", {"Aurora Peak", "◆ ยอดเขา — มองเห็นทั้งหมู่บ้าน"}},
        {"cave", {"Crystal Cavern", "◆ ถ้ำคริสตัล"}},
        // medieval citadel
        {"citadel", {"The Royal Citadel", "◆ ปราสาทหลวง — พระราชวังหินสูงตระหง่าน"}},
        {"market", {"Grand Courtyard & Market", "◆ ลานตลาดโบราณและน้ำพุหลวง"}},
        {"drawbridge", {"The Drawbridge & Moat", "◆ สะพานข้ามคูเมือง — ประตูสู่อาณาจักร"}},
        {"tavern", {"The Gilded Griffin Tavern", "◆ โรงเตี๊ยมกริฟฟิน — กลิ่นหอมของซุปอุ่นและฟืน"}},
        {"ramparts", {"Castle Ramparts", "◆ เชิงเทินกำแพงปราสาท — ทางเดินตรวจการณ์รอบเมือง"}},
        {"garden", {"The Royal Rose Garden", "◆ สวนกุหลาบหลวง — ดอกไม้บานสะพรั่ง"}},
        {"archery", {"Knight's Training Grounds", "◆ ลานฝึกอัศวินและเป้ายิงธนู"}},
        {"belltower", {"The Bell Tower", "◆ หอระฆังโบราณ"}},
        {"portal", {"The Ancient Gateway", "◆ ซุ้มประตูกาลเวลา — ทางเชื่อมสู่ดินแดนคู่ขนาน"}},
        {"swanlake", {"Swan Lake", "◆ ทะเลสาบหงส์ — ขึ้นเรือแล้วพายเล่นได้"}},
        // the wilds between the two towns (new)
        {"oldroad", {"The Old Road", "◆ ถนนสายเก่า — เชื่อมหมู่บ้านหิมะกับเมืองยุคกลาง"}},
        {"riverside", {"Silver River", "◆ แม่น้ำเงิน — พายเรือล่องไปถึงอีกเมืองได้"}},
        {"spring", {"Spring Pond", "◆ บ่อน้ำพุ — ท่าเรือฝั่งหมู่บ้านหิมะ"}},
        {"harbour", {"Citadel Harbour", "◆ ท่าเรือหน้าเมืองยุคกลาง"}},
        {"camp", {"Wayfarer's Camp", "◆ จุดพักกลางทาง — ผิงไฟอุ่นๆ"}},
        {"bridge", {"Willow Bridge", "◆ สะพานไม้ข้ามแม่น้ำเงิน — อีกฝั่งมีพุ่มเบอร์รี่"}},
        {"pinewood", {"Snowline Pines", "◆ ป่าสนแนวหิมะ"}},
        {"oakwood", {"Greenwood", "◆ ป่าโอ๊คเขียวขจี"}},
    };
    return z;
}

struct TimeInfo {
    std::string icon, name, clock;
};
inline TimeInfo timeInfo(const std::string& t) {
    if (t == "day") return {"☀", "Bright morning", "10:20 · CLEAR DAY"};
    if (t == "dusk") return {"◒", "Golden hour", "17:24 · TWILIGHT"};
    return {"☾", "Silent night", "23:40 · STARRY NIGHT"};
}

}  // namespace config
