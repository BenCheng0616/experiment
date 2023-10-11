**<center>RDMA實驗操作方法</center>**
1. 建立 RDMA 環境
    ```
    apt update
    apt install -y cmake gcc
    apt install -y git make vim perftest
    apt install -y ibverbs-providers ibverbs-utils libibverbs1 librdmacm1 libibumad3 rdma-core rdmacm-utils librdmacm-dev libibverbs-dev libibumad-dev
    modprobe rdma_rxe
    modinfo rdma_rxe
    rdma link add rxe_0 type rxe netdev ens33
    ```
# perftest是RDMA版的iperf
2. 安裝 Docker, Docker Compose
    ```
    apt update
    apt install -y ca-certificates curl gnupg
    install -m 0755 -d /etc/apt/keyrings
    curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
    chmod a+r /etc/apt/keyrings/docker.gpg
    echo "deb [arch="$(dpkg --print-architecture)" signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu \
    "$(. /etc/os-release && echo "$VERSION_CODENAME")" stable" | \
    sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
    apt update
    apt install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
    ```
3. 建立並進入rdma容器
    ```
    docker run --privileged --network host --device=/dev/infiniband/uverbs0 --device=/dev/infiniband/rdma_cm -dit --name rdma_container1 myrdma

    docker exec rdma_container1

    apt update
    apt install ibverbs-providers ibverbs-utils libibverbs-dev libibverbs1 librdmacm-dev librdmacm1 rdmacm-utils libibumad-dev libibumad3 rdma-core perftest
    ```

4. 編譯
    ```
    gcc -o server server.cpp -lrdmacm -libverbs -lstdc++
    ```

5. wireshark過濾
    ```
    (ip.src == 192.168.30.130 or ip.src == 192.168.30.131 or ip.src == 192.168.30.132) and (ip.dst == 192.168.30.130 or ip.dst == 192.168.30.131 or ip.dst == 192.168.30.132)
    ```

6. VM建立共享資料夾
    ```
    sudo vmhgfs-fuse .host:/ /mnt/hgfs -o allow_other -o uid=1000 -o gid=1000 -o umask=022
    ```