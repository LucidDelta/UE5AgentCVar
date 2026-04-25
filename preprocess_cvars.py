import json, re, os
from datetime import date

src = r"C:\Users\LucidDelta\Downloads\AllUE55Cvars.txt"
dst = r"C:\Users\LucidDelta\Documents\Unreal Projects\WarehouseGame\Plugins\UE5AgentCVar\Resources\cvars.json"
os.makedirs(os.path.dirname(dst), exist_ok=True)

# Common English stopwords + UE-doc filler that adds no search signal.
STOPWORDS = {
    # Articles, conjunctions, prepositions, pronouns
    "the","a","an","and","or","but","is","are","was","were","be","been","being",
    "to","of","in","on","for","with","at","by","from","as","into","onto","off",
    "this","that","these","those","it","its","their","there","here","when","then",
    "if","else","not","no","yes","do","does","did","done","can","could","will",
    "would","should","may","might","must","shall","has","have","had","having",
    "any","all","some","each","every","other","more","less","most","least",
    "very","too","also","only","just","both","either","neither","such","same",
    "still","even","otherwise",
    # Generic verbs / interrogatives
    "use","used","using","uses","via","per","etc","eg","ie",
    "how","why","what","which","who","whom","whose",
    # Boilerplate metadata words
    "value","values","set","sets","setting","settings","get","gets","getting",
    "true","false","enable","disable","enabled","disabled","enables","toggle",
    # UE-doc filler: high-frequency words that carry no domain meaning
    "default","whether","allows","allow","allowed","many","useful","specific",
    "works","work","put","like","run","running","before","after","shown",
    "shows","abc","note","example","see","note","optional","valid",
}

# Splits a CVar name into meaningful tokens by:
#   1. breaking on . _ - first
#   2. splitting each chunk on CamelCase / number boundaries
#      (ABCDef -> ABC, Def ; fooBar123 -> foo, Bar, 123)
CAMEL_RE = re.compile(r'[A-Z]+(?=[A-Z][a-z])|[A-Z]?[a-z]+|[A-Z]+|\d+')

def split_name(name: str):
    out = []
    for chunk in re.split(r'[.\-_]', name):
        if not chunk:
            continue
        for tok in CAMEL_RE.findall(chunk):
            t = tok.lower()
            if len(t) >= 2 and t not in STOPWORDS:
                out.append(t)
    return out

# Strips punctuation, lowercases, drops short and stopword tokens.
WORD_RE = re.compile(r"[A-Za-z][A-Za-z0-9]+")

def split_description(desc: str):
    out = []
    for w in WORD_RE.findall(desc):
        t = w.lower()
        if len(t) >= 3 and t not in STOPWORDS:
            out.append(t)
    return out

cvars = []
current_section = "General"

with open(src, encoding="utf-8") as f:
    for line in f:
        line = line.rstrip("\n")
        parts = line.split("\t")
        if len(parts) == 1:
            candidate = parts[0].strip()
            if candidate and candidate != "Variable" and "Default Value" not in candidate:
                current_section = candidate
        elif len(parts) >= 3:
            name, default, desc = parts[0].strip(), parts[1].strip(), "\t".join(parts[2:]).strip()
            if name == "Variable":
                continue
            kw_name = split_name(name)
            kw_desc = split_description(desc)
            keywords = list(dict.fromkeys(kw_name + kw_desc))
            cvars.append({
                "name": name,
                "default": default,
                "description": desc,
                "section": current_section,
                "keywords": keywords,
            })

out = {"generated": str(date.today()), "engine_version": "5.5", "count": len(cvars), "cvars": cvars}
with open(dst, "w", encoding="utf-8") as f:
    json.dump(out, f, indent=2, ensure_ascii=False)

# Quick sanity stats — eyeball that stopwords/camel splits look right.
import collections
all_kw = [k for e in cvars for k in e["keywords"]]
top = collections.Counter(all_kw).most_common(20)
print(f"Written {len(cvars)} CVars to {dst}")
print(f"Total keyword tokens: {len(all_kw)}  unique: {len(set(all_kw))}  avg/cvar: {len(all_kw)/max(1,len(cvars)):.1f}")
print("Top 20 keywords:", top)
print("Sample entries:")
for e in cvars[:3]:
    print(f"  {e['name']}  ->  {e['keywords']}")
for e in cvars:
    if "ShadowCopyCustomData".lower() in e["name"].lower() or "ComponentRecycle".lower() in e["name"].lower():
        print(f"  {e['name']}  ->  {e['keywords']}")
        break
