"""Verify exact official IDF source assumptions. Read-only, no SDK execution/network."""
import argparse
import hashlib
from pathlib import Path

COMMIT = 'b774170ff46'
HASHES = {
    'gatt_sr.c': '860e093058f501462c178e5405673400bdc375e222beac7da57116eee933ab92',
    'gatt_db.c': 'f165f5fd79863dd4184ddacdc5a5cbf50a6c2f2255bf063849a493d5e25df5ce',
    'gatt_api.h': '918de74c2a6ef025f74f0525b846c32048232504ac06d1b409d7d48290cb6e17',
    'gatt_utils.c': 'b3f13fcf4f4ac007ad91470a6badbbaced4aa5c3035e33233569a21380638115',
    'bta_gatts_act.c': 'd46a028f404045b3404eaddf871450b0ac718b4ac77be759b2bef17348a6c37d',
    'btc_gatts.c': 'eadc5f917c2508c75110a32e7c220d2353a8f6117080c947a26424adbc4513b6',
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source_dir', type=Path)
    args = parser.parse_args()
    sources = {}
    for name, digest in HASHES.items():
        data = (args.source_dir / name).read_bytes()
        assert hashlib.sha256(data).hexdigest() == digest, f'{name}: source differs from audited commit'
        sources[name] = data.decode('utf-8')
    sr = sources['gatt_sr.c']
    normal = sr[sr.index('void gatts_process_write_req ('):sr.index('void gatt_attr_process_prepare_write (')]
    assert normal.index('gatts_write_attr_perm_check (') < normal.index('if (status == GATT_SUCCESS)') < normal.index('gatts_write_attr_value_by_handle') < normal.index('gatt_sr_send_req_callback(')
    prepare = sr[sr.index('void gatt_attr_process_prepare_write ('):]
    assert prepare.index('gatts_write_attr_perm_check (') < prepare.index('GATT_RSP_BY_APP') < prepare.index('gatt_sr_send_req_callback(')
    db = sources['gatt_db.c']
    permission = db[db.index('tGATT_STATUS gatts_write_attr_perm_check ('):]
    assert '(perm & GATT_WRITE_ENCRYPTED_PERM ) && !(sec_flag & GATT_SEC_FLAG_ENCRYPTED)' in permission
    assert 'GATT_INSUF_ENCRYPTION' in permission
    mask = sources['gatt_api.h'].split('#define GATT_WRITE_ALLOWED', 1)[1].split('\n\n', 1)[0]
    assert 'GATT_PERM_WRITE_ENCRYPTED' in mask
    assert 'BTM_GetSecurityFlagsByTransport' in sources['gatt_utils.c']
    assert 'btc_gatts_cb_to_app(ESP_GATTS_WRITE_EVT' in sources['btc_gatts.c']
    print(f'PASS: exact IDF {COMMIT} source hashes and encrypted-permission-before-callback/BY_APP ordering')


if __name__ == '__main__':
    main()
