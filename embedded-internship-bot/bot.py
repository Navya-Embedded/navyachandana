import requests
from bs4 import BeautifulSoup
import json
from datetime import datetime

KEYWORDS = [
    "embedded",
    "firmware",
    "embedded software",
    "rtos",
    "device software"
]

QUERIES = [
    "embedded systems intern",
    "firmware intern",
    "embedded software intern",
    "rtos intern"
]

def load_seen_jobs():
    try:
        with open("jobs_seen.json", "r") as f:
            return json.load(f)
    except:
        return []

def save_seen_jobs(jobs):
    with open("jobs_seen.json", "w") as f:
        json.dump(jobs, f)

def fetch_jobs():

    jobs = []

    for query in QUERIES:

        url = f"https://www.indeed.com/jobs?q={query}"

        page = requests.get(url)

        soup = BeautifulSoup(page.text, "html.parser")

        for job in soup.select(".job_seen_beacon"):

            title = job.select_one("h2").text.strip()
            company = job.select_one(".companyName").text.strip()
            location = job.select_one(".companyLocation").text.strip()

            link = "https://www.indeed.com" + job.select_one("a")["href"]

            if any(k in title.lower() for k in KEYWORDS):

                jobs.append({
                    "title": title,
                    "company": company,
                    "location": location,
                    "link": link
                })

    return jobs


def remove_duplicates(jobs, seen):

    new_jobs = []

    for job in jobs:

        key = job["title"] + job["company"]

        if key not in seen:
            new_jobs.append(job)
            seen.append(key)

    return new_jobs, seen


def create_post(jobs):

    post = "🚀 Embedded Systems & Firmware Internship Updates (USA)\n\n"

    for i, job in enumerate(jobs[:10]):

        post += f"{i+1}. {job['company']} – {job['title']}\n"
        post += f"📍 Location: {job['location']}\n"
        post += f"🔗 Apply: {job['link']}\n\n"

    post += "Know other embedded internships? Share them below.\n\n"
    post += "#EmbeddedSystems #Firmware #EmbeddedJobs #Internships"

    return post


seen = load_seen_jobs()

jobs = fetch_jobs()

jobs, seen = remove_duplicates(jobs, seen)

save_seen_jobs(seen)

post = create_post(jobs)

with open("daily_post.txt", "w") as f:
    f.write(post)

print(post)