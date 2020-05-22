# CS438 MP3 Reliable Transmission
Winner of all three tests class CS438 Spring 2020.

```bash
65MB - 100Mb Link 3.8s
1MB  - 100Mb Link 0.14s
1MB  - 100Mb Link 0.28s
```

## TODO
- [ ] Handshake
- [ ] Calculate framze size during handshake
- [ ] Calculate transmission rate after handshake
- [ ] Mechanism to reduce duplicate packets

## NOTE
I won the competition by rate-based implementation which takes the advantage of the system. Chances are that YOUR professor will ***NOT*** favor this approach. And I personally DO NOT agree that a standalone server would not work under rate-based protocol without congestion control.