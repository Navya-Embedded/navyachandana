import requests
import xml.etree.ElementTree as ET
import json
import os

SEEN_FILE = "jobs_seen.json"
POST_FILE = "daily_post.txt"

SEARCH_QUERIES = [
    "embedded software intern",
    "firmware intern",
    "embedded systems intern",
    "hardware intern",
    "computer engineering intern"
]


# ------------------------
# LOAD SEEN JOBS
# ------------------------

def load_seen():
    if os.path.exists(SEEN_FILE):
        with open(SEEN_FILE, "r") as f:
            return set(json.load(f))
    return set()


def save_seen(seen):
    with open(SEEN_FILE, "w") as f:
        json.dump(list(seen), f)


# ------------------------
# FETCH INDEED JOBS
# ------------------------

def fetch_indeed():

    jobs = []

    for query in SEARCH_QUERIES:

        q = query.replace(" ", "+")

        url = f"https://www.indeed.com/jobs?q=embedded&l=United+States"

        try:
            response = requests.get(url, timeout=10)

            if response.status_code != 200:
                continue

            root = ET.fromstring(response.content)

            for item in root.findall(".//item"):

                title = item.find("title").text
                link = item.find("link").text

                jobs.append({
                    "id": link,
                    "title": title,
                    "link": link
                })

        except:
            continue

    return jobs


# ------------------------
# FILTER DUPLICATES
# ------------------------

def filter_jobs(jobs, seen):

    filtered = []

    for job in jobs:

        if job["id"] not in seen:

            filtered.append(job)
            seen.add(job["id"])

    return filtered


# ------------------------
# GENERATE POST
# ------------------------

def generate_post(jobs):

    header = "🚀 Embedded Systems & Firmware Internship Updates (USA)\n\n"

    footer = "\nKnow other embedded internships? Share them below.\n\n#EmbeddedSystems #Firmware #EmbeddedJobs #Internships"

    if not jobs:
        return header + "Few results found today. Try searching manually on LinkedIn.\n\n" + footer

    post = header

    for i, job in enumerate(jobs[:10], 1):

        post += f"{i}. {job['title']}\n"
        post += f"🔗 Apply: {job['link']}\n\n"

    post += footer

    return post


# ------------------------
# MAIN
# ------------------------

def main():

    print("Running Internship Finder Bot...")

    seen = load_seen()

    jobs = fetch_indeed()

    filtered = filter_jobs(jobs, seen)

    post = generate_post(filtered)

    with open(POST_FILE, "w", encoding="utf-8") as f:
        f.write(post)

    save_seen(seen)

    print("\nGenerated Post:\n")
    print(post)


if __name__ == "__main__":
    main()