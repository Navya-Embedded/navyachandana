from selenium import webdriver
from selenium.webdriver.common.by import By
from webdriver_manager.chrome import ChromeDriverManager
import time

LINKEDIN_EMAIL = "navya03chandana@gmail.com"
LINKEDIN_PASSWORD = "kamakshiNAV@123"

GROUP_URL = "https://www.linkedin.com/groups/19271001/"

driver = webdriver.Chrome(ChromeDriverManager().install())

driver.get("https://www.linkedin.com/login")

time.sleep(3)

driver.find_element(By.ID, "username").send_keys(LINKEDIN_EMAIL)
driver.find_element(By.ID, "password").send_keys(LINKEDIN_PASSWORD)

driver.find_element(By.XPATH, "//button[@type='submit']").click()

time.sleep(5)

driver.get(GROUP_URL)

time.sleep(5)

with open("daily_post.txt") as f:
    post_text = f.read()

post_box = driver.find_element(By.XPATH, "//div[contains(@class,'share-box')]")
post_box.click()

time.sleep(3)

editor = driver.find_element(By.XPATH, "//div[@role='textbox']")
editor.send_keys(post_text)

time.sleep(2)

post_button = driver.find_element(By.XPATH, "//button[contains(@class,'share-actions__primary-action')]")
post_button.click()

print("Post published successfully!")
