import pytest
import requests
import time

proxies = {
    "http": "http://proxy:12345",
    "https": "http://proxy:12345",
}

host = "http://server:5000"


def test_cache():
    url = host+"/valid-cache"
    headers = {
    }
    response = requests.get(url, proxies=proxies, headers=headers)

    print("\nStatus Code:", response.status_code)  
    print("Response Body:", response.text)

    assert response.status_code == 200
    assert "message" in response.headers
    assert "v1" in response.headers["message"]


    response = requests.get(url, proxies=proxies, headers=headers)

    print("\nStatus Code:", response.status_code)  
    print("Response Body:", response.text)

    assert response.status_code == 200
    assert "message" in response.headers
    assert "v1" in response.headers["message"]

def test_revalid_cache():
    url = host+"/revalid-cache"
    headers = {
        'Cache-Control': 'no-cache'
    }
    response = requests.get(url, proxies=proxies, headers=headers)

    print("\nStatus Code:", response.status_code)  
    print("Response Body:", response.text)

    assert response.status_code == 200
    assert "ETag" in response.headers
    assert "v1" in response.headers["ETag"]


    response = requests.get(url, proxies=proxies, headers=headers)

    print("\nStatus Code:", response.status_code)  
    print("Response Body:", response.text)

    assert response.status_code == 200
    assert "ETag" in response.headers
    assert "v1" in response.headers["ETag"]

    response = requests.get(url, proxies=proxies, headers=headers)

    print("\nStatus Code:", response.status_code)  
    print("Response Body:", response.text)

    assert response.status_code == 200
    assert "ETag" in response.headers
    assert "v2" in response.headers["ETag"]


def test_only_if_cached():
    url = host+"/cache/only-if-cached"
    
    response = requests.get(url, proxies=proxies)

    print("\nStatus Code:", response.status_code)  
    print("Response Body:", response.text)

    assert response.status_code == 200
    assert "message" in response.headers
    assert "v1" in response.headers["message"]

    headers = {
        'Cache-Control': 'only-if-cached'
    }
    response = requests.get(url, proxies=proxies, headers=headers)

    print("\nStatus Code:", response.status_code)  
    print("Response Body:", response.text)

    assert response.status_code == 200
    assert "message" in response.headers
    assert "v1" in response.headers["message"]


def test_max_age_cache():
    url = host+"/cache/max-age"
    headers = {
        'Cache-Control': 'max-age=10'
    }
    response = requests.get(url, proxies=proxies, headers=headers)

    print("\nStatus Code:", response.status_code)  
    print("Response Body:", response.text)

    assert response.status_code == 200
    assert "message" in response.headers
    assert "v1" in response.headers["message"]


    response = requests.get(url, proxies=proxies, headers=headers)

    print("\nStatus Code:", response.status_code)  
    print("Response Body:", response.text)

    assert response.status_code == 200
    assert "message" in response.headers
    assert "v1" in response.headers["message"]

    time.sleep(1)
    headers2 = {
        'Cache-Control': 'max-age=0'
    }
    response = requests.get(url, proxies=proxies, headers=headers2)

    print("\nStatus Code:", response.status_code)  
    print("Response Body:", response.text)

    assert response.status_code == 200
    assert "message" in response.headers
    assert "v2" in response.headers["message"]

def test_cache_no_store():
    url = host+"/cache/no-store"
    headers = {
        'Cache-Control': 'no-store'
    }
    response = requests.get(url, proxies=proxies, headers=headers)

    print("\nStatus Code:", response.status_code)  
    print("Response Body:", response.text)

    assert response.status_code == 200
    assert "message" in response.headers
    assert "v1" in response.headers["message"]


    response = requests.get(url, proxies=proxies, headers=headers)

    print("\nStatus Code:", response.status_code)  
    print("Response Body:", response.text)

    assert response.status_code == 200
    assert "message" in response.headers
    assert "v2" in response.headers["message"]

def test_cache_min_fresh():
    url = host+"/cache/min-fresh"
    headers = {
        'Cache-Control': 'min-fresh=10'
    }
    response = requests.get(url, proxies=proxies, headers=headers)

    print("\nStatus Code:", response.status_code)  
    print("Response Body:", response.text)

    assert response.status_code == 200
    assert "message" in response.headers
    assert "v1" in response.headers["message"]


    response = requests.get(url, proxies=proxies, headers=headers)

    print("\nStatus Code:", response.status_code)  
    print("Response Body:", response.text)

    assert response.status_code == 200
    assert "message" in response.headers
    assert "v2" in response.headers["message"]

def test_cache_max_stale():
    url = host+"/cache/max-stale"
    headers = {
        'Cache-Control': 'max-stale=5'
    }

    response = requests.get(url, proxies=proxies, headers=headers)

    print("\nStatus Code:", response.status_code)  
    print("Response Body:", response.text)

    assert response.status_code == 200
    assert "message" in response.headers
    assert "v1" in response.headers["message"]


    response = requests.get(url, proxies=proxies, headers=headers)

    print("\nStatus Code:", response.status_code)  
    print("Response Body:", response.text)

    assert response.status_code == 200
    assert "message" in response.headers
    assert "v1" in response.headers["message"]

def test_cache_response_no_cache():
    url = host+"/cache/response-no-cache"
    response = requests.get(url, proxies=proxies)

    print("\nStatus Code:", response.status_code)  
    print("Response Body:", response.text)

    assert response.status_code == 200
    assert "message" in response.headers
    assert "v1" in response.headers["message"]


    response = requests.get(url, proxies=proxies)

    print("\nStatus Code:", response.status_code)  
    print("Response Body:", response.text)

    assert response.status_code == 200
    assert "message" in response.headers
    assert "v2" in response.headers["message"]

def test_cache_must_revalidate():
    url = host+"/cache/must-revalidate"
    response = requests.get(url, proxies=proxies)

    print("\nStatus Code:", response.status_code)  
    print("Response Body:", response.text)

    assert response.status_code == 200
    assert "message" in response.headers
    assert "v1" in response.headers["message"]


    response = requests.get(url, proxies=proxies)

    print("\nStatus Code:", response.status_code)  
    print("Response Body:", response.text)

    assert response.status_code == 200
    assert "message" in response.headers
    assert "v2" in response.headers["message"]

def test_response_no_store():
    url = host+"/cache/response-no-store"
    response = requests.get(url, proxies=proxies)

    print("\nStatus Code:", response.status_code)  
    print("Response Body:", response.text)

    assert response.status_code == 200
    assert "message" in response.headers
    assert "v1" in response.headers["message"]

    time.sleep(1)
    response = requests.get(url, proxies=proxies)

    print("\nStatus Code:", response.status_code)  
    print("Response Body:", response.text)

    assert response.status_code == 200
    assert "message" in response.headers
    assert "v2" in response.headers["message"]
    


if __name__ == "__main__":
    pass
    # test_response_no_store()