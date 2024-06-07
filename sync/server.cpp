#include "common.hpp"

map<string, map<string, uint64_t>> send_rec;

int read_client_list()
{
    ifstream file("client.txt");
    if (file.is_open())
    {
        string ip;
        while (getline(file, ip))
        {
            send_rec[ip] = map<string, uint64_t>{};
        }
        file.close();
    }
    else
    {
        cerr << "unable to open file." << endl;
        return -1;
    }
    return 0;
}

int write_send_record()
{
    std::ofstream outputFile("output.txt");

    if (outputFile.is_open())
    {
        for (const auto &outerPair : send_rec)
        {
            outputFile << outerPair.first << ":" << endl;
            for (const auto &innerPair : outerPair.second)
                outputFile << "\t" << innerPair.first << ": " << innerPair.second << endl;
            outputFile << endl;
        }
        outputFile.close();
    }
    else
    {
        cerr << "unable to open file for writing." << endl;
        return -1;
    }
    return 0;
}

// set value of send_rec to zero
void set_rec_zero()
{
    for (auto &outer : send_rec)
        for (auto &inner : outer.second)
            inner.second = 0;
}

void add_send_rec(Amount *amount, string &ip)
{
    for (int j = 0; j < GID_NUM; j++)
    {
        if (strcmp(amount[j].gid, "\0") == 0)
            break;
        string dip(amount[j].gid);
        send_rec[ip][dip] += amount[j].tx;
        send_rec[dip][ip] += amount[j].rx;
    }
}

int main()
{
    int ret, sock_fd;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));

    // read client list
    ret = read_client_list();
    ret = read_server_list();
    if (ret < 0)
    {
        cerr << "cannot open client.txt or server.txt" << endl;
        exit(1);
    } else {
        cerr << "open client.txt or server.txt successfully" << endl;
    }
    int client_num = send_rec.size();
    struct sockaddr_in client_addr[client_num];

    // socket create and verification
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1)
    {
        cerr << "socket creation failed..." << endl;
        exit(1);
    } else {
        cerr << "socket creation success..." << endl;
    }

    // assign IP, PORT
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    // bind and listen
    if ((bind(sock_fd, (struct sockaddr *)&server_addr, addr_len)) != 0)
    {
        cerr << "socket bind failed..." << endl;
        exit(1);
    }
    listen(sock_fd, client_num);

    // build the connection with clients
    map<int, string> fd_to_ip;
    vector<int> fds;
    for (int i = 0; i < client_num; i++)
    {
        int connect_fd = accept(sock_fd, (struct sockaddr *)&client_addr[i], &addr_len);
        if (connect_fd < 0)
        {
            cerr << "server accept failed..." << endl;
            exit(1);
        } else {
            cerr << "server accept success..." << endl;
        }
        fds.push_back(connect_fd);
        fd_to_ip[connect_fd] = inet_ntoa(client_addr[i].sin_addr);
    }

    // receive traffic prediction
    alloc_shared_memory();
    auto start = chrono::high_resolution_clock::now();
    chrono::microseconds duration;
    do
    {
        set_rec_zero();
        add_send_rec(shm->amount, server_ip);
        for (int i = 0; i < client_num; i++)
        {
            Amount *cli_amount = (Amount *)calloc(sizeof(Amount), GID_NUM);
            string ip = fd_to_ip[fds[i]];
            if (sock_read(fds[i], (void *)cli_amount, amount_size) < 0)
                cerr << "socket read failed..." << endl;
            add_send_rec(cli_amount, ip);
        }
        ret = write_send_record();
        if (ret < 0)
            exit(1);
        auto end = chrono::high_resolution_clock::now();
        duration = chrono::duration_cast<chrono::microseconds>(end - start);
    } while (duration.count() < TIME);
    release_shared_memory();

    // close socket
    close(sock_fd);
    for (const auto &fd : fds)
        close(fd);
    return 0;
}